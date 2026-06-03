#include "Render.h"
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "ObjLoader.h"
#include "Texture.h"
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <random>
#include <cmath>
#include "debout.h"

// Внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;



// Кривая Безье 3-й степени)
struct Vec3 { double x, y, z; };
GLUquadric* fishQuadric = nullptr;
std::random_device rd;
std::mt19937 global_gen(rd());

// Базисные полиномы Бернштейна для степени 3
Vec3 evalCubicBezier(double t, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3)
{
    double u = 1.0 - t;
    double uu = u * u;
    double tt = t * t;
    double uuu = uu * u;
    double ttt = tt * t;

    return {
        uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x,
        uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y,
        uuu * p0.z + 3 * uu * t * p1.z + 3 * u * tt * p2.z + ttt * p3.z
    };
}

// Производная касательная для вычисления направления полёта
Vec3 evalCubicBezierDeriv(double t, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3)
{
    double u = 1.0 - t;
    return {
        3 * u * u * (p1.x - p0.x) + 6 * u * t * (p2.x - p1.x) + 3 * t * t * (p3.x - p2.x),
        3 * u * u * (p1.y - p0.y) + 6 * u * t * (p2.y - p1.y) + 3 * t * t * (p3.y - p2.y),
        3 * u * u * (p1.z - p0.z) + 6 * u * t * (p2.z - p1.z) + 3 * t * t * (p3.z - p2.z)
    };
}

// Контрольные точки траектории
const Vec3 BEZIER_PATH[4] = {
    {  0.0,  8.0, -25.0 },
    { 25.0, 10.0,   0.0 },
    {  0.0,  7.0,  25.0 },
    {-25.0,  9.0,   0.0 }
};
bool texturing = true;
bool lightning = true;
bool alpha = false;
bool cameraFollowMode = true;

enum GameState { PLAYING, GAME_OVER, WIN };
GameState gameState = PLAYING;

// Умножение матриц c[M1][N1] = a[M1][N1] * b[M2][N2]
template <typename T, int M1, int N1, int M2, int N2> void MatrixMultiply(const T* a, const T* b, T* c)
{
    for (int i = 0; i < M1; ++i)
    {
        for (int j = 0; j < N2; ++j)
        {
            c[i * N2 + j] = T(0);
            for (int k = 0; k < N1; ++k)
            {
                c[i * N2 + j] += a[i * N1 + k] * b[k * N2 + j];
            }
        }
    }
}

// Текстовый прямоугольник в верхнем правом углу.
// OGL не предоставляет возможности для хранения текста;
// внутри этого класса создается картинка с текстом (через GDI),
// в виде текстуры накладывается на прямоугольник и рисуется на экране.
// Это самый простой, но очень неэффективный способ написать что-либо на экране.
GuiTextRectangle text;

// ID для текстуры
GLuint texId;

ObjModel penguinModel, iceModel, berd;

Shader cassini_sh;
Shader phong_sh;
Shader vb_sh;
Shader simple_texture_sh;

Texture penguinTex, ice_tex, water_tex, berd_tex;

struct { double x = 1.0, y = 1.0, z = 0.0; } penguinPos;
double penguinRotation = 0.0;  // поворот
double penguinSpeed = 0.0;     // скорость

const double MAX_SPEED = 1.12;
const double PENGUIN_ACCEL = 0.003;
const double FRICTION = 0.95;
const double TURN_SPEED = 1.5;

// уровни и рыбки
struct Fish {
    double x, y, z;      // координаты
    bool collected;      // собрана или нет
};

const int MAX_FISH = 30;
Fish fish[MAX_FISH];
int activeFish = 0;
int collectedFish = 0;

struct Hole {
    double x, y;      // позиция
    double radius;    // радиус отверстия
};

const int MAX_HOLES = 50;
Hole holes[MAX_HOLES];
int numHoles = 0;

int currentLevel = 1;                 // текущий уровень
const int FISH_PER_LEVEL[] = { 0, 5, 10, 15 };
void spawnFishForLevel(int level, std::mt19937& gen);
void generateHoles(int level, std::mt19937& gen);
// Управление
void handleInput(OpenGL* sender, KeyEventArg arg)
{
    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));
    if (key == 'W' || key == 'w' || key == 'ц' || key == 'Ц')
        penguinSpeed -= PENGUIN_ACCEL;

    if (key == 'S' || key == 's' || key == 'ы' || key == 'Ы')
        penguinSpeed += PENGUIN_ACCEL;

    if (key == 'A' || key == 'a' || key == 'ф' || key == 'Ф') {
        penguinRotation += TURN_SPEED;
    }

    if (key == 'D' || key == 'd' || key == 'в' || key == 'В') {
        penguinRotation -= TURN_SPEED;
    }

    if (key == 'L' || key == 'l' || key == 'д' || key == 'Д')
        lightning = !lightning;

    if (key == 'T' || key == 't' || key == 'е' || key == 'Е')
        texturing = !texturing;

    if (key == 'R' || key == 'r' || key == 'к' || key == 'К')
        alpha = !alpha;

    if (key == 'C' || key == 'c' || key == 'с' || key == 'С')
        cameraFollowMode = !cameraFollowMode;

    if (gameState == GAME_OVER || gameState == WIN) {
        if (key == VK_RETURN) {
            gameState = PLAYING;
            currentLevel = 1;
            collectedFish = 0;
            penguinPos = { 1.0, 1.0, 0.0 };
            penguinSpeed = 0;
            penguinRotation = 0;

            generateHoles(currentLevel, global_gen);
            spawnFishForLevel(currentLevel, global_gen);
            cameraFollowMode = true;
        }
        if (key == VK_ESCAPE) PostQuitMessage(0);
        return;
    }
}

// Случайное число от min до max
double randomRange(double min, double max, std::mt19937& gen)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(gen);
}

// Генерирует рыбок для указанного уровня
void spawnFishForLevel(int level, std::mt19937& gen)
{
    activeFish = FISH_PER_LEVEL[level];
    if (activeFish > MAX_FISH) activeFish = MAX_FISH;
    collectedFish = 0;

    for (int i = 0; i < activeFish; i++) {
        bool valid = false;
        int attempts = 0;

        while (!valid && attempts < 100) {
            fish[i].x = randomRange(-49, 49, gen);
            fish[i].z = 0.1;
            fish[i].y = randomRange(-4, 49, gen);
            fish[i].collected = false;

            // проверка на близость к пингвину
            double dx = fish[i].x - penguinPos.x;
            double dy = fish[i].y - penguinPos.y;
            double distToPenguin = sqrt(dx * dx + dy * dy);

            // проверка на лунку
            bool inHole = false;
            for (int h = 0; h < numHoles; h++) {
                double hdx = fish[i].x - holes[h].x;
                double hdy = fish[i].y - holes[h].y;
                double distToHole = sqrt(hdx * hdx + hdy * hdy);

                if (distToHole < holes[h].radius + 1.0) {
                    inHole = true;
                    break;
                }
            }

            // Принимаем позицию, если далеко и от пингвина, и от всех лунок
            if (distToPenguin > 8.0 && !inHole) {
                valid = true;
            }
            attempts++;
        }

        // Если не удалось найти хорошее место — ставим в безопасную зону
        if (!valid) {
            fish[i].x = randomRange(-30, 30, gen);
            fish[i].y = randomRange(10, 30, gen);
            fish[i].z = 0.1;
            fish[i].collected = false;
        }
    }
}

// Генерирует случайные отверстия во льду
void generateHoles(int level, std::mt19937& gen)
{
    // Количество отверстий зависит от уровня
    int minHoles, maxHoles;
    switch (level) {
    case 1: minHoles = 18;  maxHoles = 22; break;
    case 2: minHoles = 25; maxHoles = 30; break;
    case 3: minHoles = 32; maxHoles = 38; break;
    default: minHoles = 10; maxHoles = 15;
    }

    // Распределения по всей поверхности льда
    std::uniform_real_distribution<double> distX(-47.5, 47.5);
    std::uniform_real_distribution<double> distY(-2.5, 47.5);
    std::uniform_real_distribution<double> distRadius(0.8, 2.5);

    numHoles = minHoles + (rand() % (maxHoles - minHoles + 1));

    for (int i = 0; i < numHoles; i++) {
        bool valid = false;
        int attempts = 0;
        if (numHoles > MAX_HOLES) numHoles = MAX_HOLES;
        while (!valid && attempts < 50) {
            holes[i].x = distX(gen);
            holes[i].y = distY(gen);
            holes[i].radius = distRadius(gen);

            // 1. Не слишком близко к старту пингвина
            double dx = holes[i].x - 1.0;
            double dy = holes[i].y - 1.0;
            double distToStart = sqrt(dx * dx + dy * dy);

            // 2. Не слишком близко к другим отверстиям
            bool tooCloseToOther = false;
            for (int j = 0; j < i; j++) {
                double dx2 = holes[i].x - holes[j].x;
                double dy2 = holes[i].y - holes[j].y;
                double minDist = holes[i].radius + holes[j].radius + 1.0;
                if (sqrt(dx2 * dx2 + dy2 * dy2) < minDist) {
                    tooCloseToOther = true;
                    break;
                }
            }

            if (distToStart > 8.0 && !tooCloseToOther) {
                valid = true;
            }
            attempts++;
        }

        // Если не удалось найти хорошее место
        if (!valid) {
            holes[i].x = distX(gen);
            holes[i].y = distY(gen);
            holes[i].radius = distRadius(gen);
        }
    }
}

// Выполняется один раз перед первым рендером
void initRender()
{
    // Настройка шейдеров
    cassini_sh.VshaderFileName = "shaders/v.vert";
    cassini_sh.FshaderFileName = "shaders/cassini.frag";
    cassini_sh.LoadShaderFromFile();
    cassini_sh.Compile();

    phong_sh.VshaderFileName = "shaders/v.vert";
    phong_sh.FshaderFileName = "shaders/light.frag";
    phong_sh.LoadShaderFromFile();
    phong_sh.Compile();

    vb_sh.VshaderFileName = "shaders/v.vert";
    vb_sh.FshaderFileName = "shaders/vb.frag";
    vb_sh.LoadShaderFromFile();
    vb_sh.Compile();

    simple_texture_sh.VshaderFileName = "shaders/v.vert";
    simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
    simple_texture_sh.LoadShaderFromFile();
    simple_texture_sh.Compile();

    penguinTex.LoadTexture("textures/colormap.png");
    ice_tex.LoadTexture("textures/ice.png");
    water_tex.LoadTexture("textures/water.png");
    berd_tex.LoadTexture("textures/berd.png");
    penguinModel.LoadModel("models//penguin.obj");
    berd.LoadModel("models//berd.obj");

    fishQuadric = gluNewQuadric();
    gluQuadricNormals(fishQuadric, GLU_SMOOTH);

    //иннициализация случайных рыбок
    generateHoles(currentLevel, global_gen);
    spawnFishForLevel(currentLevel, global_gen);

    
    //==============НАСТРОЙКА ТЕКСТУР================
    // 4 байта на хранение пикселя
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    //================НАСТРОЙКА КАМЕРЫ===================
    camera.caclulateCameraPos();

    // привязываем камеру к событиям "движка"
    gl.WheelEvent.reaction(&camera, &Camera::Zoom);
    gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
    gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
    gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
    gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
    //==============НАСТРОЙКА СВЕТА===========================
    // Привязываем свет к событиям "движка"
    gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
    gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
    gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
    //========================================================
    //====================Прочее==============================
    gl.KeyDownEvent.reaction(handleInput);
    text.setSize(310, 180);
    //========================================================

    camera.setPosition(2, 1.5, 1.5);
    light.SetPosition(1.252, -11, 100);
}

float view_matrix[16];
double full_time = 0;
int location = 0;

// Рисует отверстия во льду с текстурой воды
void drawHoles()
{
    for (int i = 0; i < numHoles; i++) {
        glPushMatrix();
        glTranslated(holes[i].x, holes[i].y, 0.05);
        glDisable(GL_TEXTURE_2D);
        //glDisable(GL_LIGHTING);
        glColor4f(0.02f, 0.05f, 0.15f, 1.0f);

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0, 0, 0);
        for (int j = 0; j <= 32; j++) {
            double angle = 2.0 * 3.14159 * j / 32;
            glVertex3f(cos(angle) * holes[i].radius, sin(angle) * holes[i].radius, 0);
        }
        glEnd();

        glEnable(GL_TEXTURE_2D);
        water_tex.Bind();
        glDisable(GL_BLEND);
        //glDisable(GL_LIGHTING);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        double R = holes[i].radius - 0.05;
        int n = 50;

        glNormal3d(0, 0, 1);
        glBegin(GL_TRIANGLES);

        for (int j = 0; j < n; j++)
        {
            double a1 = 2.0 * 3.141592653589793 * j / n;
            double a2 = 2.0 * 3.141592653589793 * (j + 1) / n;

            double x1 = R * cos(a1);
            double y1 = R * sin(a1);
            double x2 = R * cos(a2);
            double y2 = R * sin(a2);

            double u1 = 0.5 + x1 / (2.0 * R);
            double v1 = 0.5 + y1 / (2.0 * R);
            double u2 = 0.5 + x2 / (2.0 * R);
            double v2 = 0.5 + y2 / (2.0 * R);

            // Центр = середина текстуры
            glTexCoord2d(0.5, 0.5);
            glVertex3d(0, 0, 0.01);

            glTexCoord2d(u1, v1);
            glVertex3d(x1, y1, 0.01);

            glTexCoord2d(u2, v2);
            glVertex3d(x2, y2, 0.01);
        }

        glEnd();

        // обводка
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.8f, 0.9f, 1.0f, 1.0f);

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0, 0, 0);
        for (int j = 0; j <= 32; j++) {
            double angle = 2.0 * 3.14159 * j / 32;
            glVertex3f(cos(angle) * holes[i].radius, sin(angle) * holes[i].radius, 0);
        }
        glEnd();

        glPopMatrix();
    }
}

// Рисует одну рыбку
void drawFish(const Fish& f, double time)
{
    if (f.collected) return;  // не рисуем, если уже собрали

    glPushMatrix();
    glTranslated(f.x, f.y, f.z);
    glRotated(time * 30, 0, 1, 0);  // вращение

    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.8f, 0.1f);

    if (fishQuadric != nullptr) {
        gluSphere(fishQuadric, 0.3, 8, 6);
    }

    // Хвост треугольник
    glBegin(GL_TRIANGLES);
    glVertex3d(-0.3, 0, 0);
    glVertex3d(-0.6, 0.2, 0);
    glVertex3d(-0.6, -0.2, 0);
    glEnd();

    glEnable(GL_LIGHTING);
    glPopMatrix();
}


// Проверяет сбор рыбок и переход на следующий уровень
bool checkFishCollection()
{
    // Проверяем каждую рыбку
    for (int i = 0; i < activeFish; i++) {
        if (fish[i].collected) continue;

        // Расстояние между пингвином и рыбкой
        double dx = penguinPos.x - fish[i].x;
        double dy = penguinPos.y - fish[i].y;
        double dz = penguinPos.z - fish[i].z;
        double distance = sqrt(dx * dx + dy * dy + dz * dz);

        // Если близко — собираем!
        if (distance < 1.0) {
            fish[i].collected = true;
            collectedFish++;
        }
    }

    // Проверка: все рыбки собраны?
    if (collectedFish >= activeFish) {
        if (currentLevel < 3) {
            currentLevel++;  // переход на следующий уровень
            return true;
        }
    }
    return false;
}


// Проверяет, упал ли пингвин в отверстие
bool checkHoleCollision()
{
    for (int i = 0; i < numHoles; i++) {
        // Расстояние от пингвина до отверстия
        double dx = penguinPos.x - holes[i].x;
        double dy = penguinPos.y - holes[i].y;
        double distance = sqrt(dx * dx + dy * dy);

        // Если пингвин внутри отверстия (с запасом на его размер)
        if (distance < holes[i].radius + 0.3) { 
            return true;
        }
    }
    return false;
}

// Отрисовка экрана "Игра окончена"
void drawGameOver()
{
    // Затемняем фон
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, gl.getWidth(), 0, gl.getHeight(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Текст по центру
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, gl.getWidth(), 0, gl.getHeight(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    std::wstringstream msg;
    msg << L"\n\n\n\n╔════════════════════════════╗\n"
        << L"║           УПС!             ║\n"
        << L"║        Пингвин упал!       ║\n"
        << L"╠════════════════════════════╣\n"
        << L"║ Собрано рыбок: " << std::setw(2) << collectedFish << L"/" << std::setw(2) << activeFish << L"       ║\n"
        << L"║ Уровень: " << currentLevel << L"/3               ║\n"
        << L"╠════════════════════════════╣\n"
        << L"║ [ENTER] — начать заново    ║\n"
        << L"╚════════════════════════════╝";

    text.setSize(250, 250);
    text.setPosition(gl.getWidth() / 2 - 250, gl.getHeight() / 2 - 80);
    text.setText(msg.str().c_str()); 
    text.Draw();
    text.setSize(310, 180);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Render(double delta_time)
{
    full_time += delta_time;

    // === ПРОВЕРКА ПРОИГРЫША ===
    if (gameState == PLAYING) {
        if (checkHoleCollision()) {
            gameState = GAME_OVER;
        }
    }

    penguinSpeed *= FRICTION;  // трение
    if (fabs(penguinSpeed) < 0.0001) penguinSpeed = 0;  // остановка
    if (penguinSpeed > MAX_SPEED) penguinSpeed = MAX_SPEED;  // лимит скорости
    if (penguinSpeed < -MAX_SPEED * 0.5) penguinSpeed = -MAX_SPEED * 0.5;


    // Перемещение
    double rad = penguinRotation * 3.14159 / 180.0;
    penguinPos.x += sin(rad) * penguinSpeed;
    penguinPos.y -= cos(rad) * penguinSpeed;

    // Границы льдины
    if (penguinPos.x < -49) penguinPos.x = -49;
    if (penguinPos.x > 49) penguinPos.x = 49;
    if (penguinPos.y < -4) penguinPos.y = -4;
    if (penguinPos.y > 49) penguinPos.y = 49;


    if (gameState == PLAYING) { // Проверяем сбор только во время игры
        bool levelCompleted = checkFishCollection();

        // Если уровень пройден — перегенерируем рыбки для следующего
        if (levelCompleted) {
            generateHoles(currentLevel, global_gen);
            spawnFishForLevel(currentLevel, global_gen);

            // Сбрасываем пингвина в центр
            penguinPos = { 1.0, 1.0, 0.0 };
            penguinSpeed = 0;
            penguinRotation = 0;
        }
        else if (currentLevel == 3 && collectedFish >= activeFish) {
            gameState = WIN;
            penguinSpeed = 0;
        }
    }


    // === НАСТРОЙКА КАМЕРЫ ===

    // КАМЕРА СЛЕДИТ ЗА ПИНГВИНОМ
    if (cameraFollowMode) {
        // 1. Указываем камере, куда смотреть
        camera.setTarget(penguinPos.x, penguinPos.y, penguinPos.z);
        double lookAhead = 5.0;
        // 2. Рассчитываем углы
        double rad = penguinRotation * 3.14159 / 180.0;

        // 3. Устанавливаем параметры камеры
        camera._fi1 = rad-3.14*0.5;
        camera._fi2 = 0.35;
        camera.camDist = 8.0;

        // 4. Пересчитываем позицию
        camera.caclulateCameraPos();
    }

    // Свет из позиции камеры
    if (gl.isKeyPressed('F')) {
        light.SetPosition(camera.x(), camera.y(), camera.z());
    }

    camera.SetUpCamera();

    // Забираем матрицу MODELVIEW сразу после установки камеры,
    // так как в ней отсутствуют трансформации glRotate
    glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);

    light.SetUpLight();

    // Рисуем оси
    //gl.DrawAxes();

    glBindTexture(GL_TEXTURE_2D, 0);

    // Включаем нормализацию нормалей
    // чтобы glScaled не влияли на них.

    glEnable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    // Переключаем режимы (см void switchModes(OpenGL *sender, KeyEventArg arg))
    if (lightning)
        glEnable(GL_LIGHTING);
    if (texturing)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0); // Сбрасываем текущую текстуру
    }

    if (alpha)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //=============НАСТРОЙКА МАТЕРИАЛА==============

    // Настройка материала, все что рисуется ниже будет иметь этот материал.
    // Массивы с настройками материала
    float amb[] = {0.2, 0.2, 0.1, 1.};
    float dif[] = {0.4, 0.65, 0.5, 1.};
    float spec[] = {0.9, 0.8, 0.3, 1.};
    float sh = 0.2f * 256;

    // Фоновая
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    // Дифузная
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    // Зеркальная
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    // Размер блика
    glMaterialf(GL_FRONT, GL_SHININESS, sh);

    // Сглаживание освещения
    glShadeModel(GL_SMOOTH); // закраска по Гуро
                             //(GL_SMOOTH - плоская закраска)
                             // 
    //============ РИСОВАТЬ ТУТ ==============

    glActiveTexture(GL_TEXTURE0);
    ice_tex.Bind();

    glPushMatrix();

    glTranslated(1.2, 0, 0);

    glBegin(GL_QUADS);
    glNormal3d(0, 0, 1);
    glTexCoord2d(1, 1);
    glVertex3d(50, -5, 0);
    glTexCoord2d(1, 0);
    glVertex3d(50, 50, 0);
    glTexCoord2d(0, 0);
    glVertex3d(-50, 50, 0);
    glTexCoord2d(0, 1);
    glVertex3d(-50, -5, 0);
    glEnd();

    glPopMatrix();

    drawHoles();



    // === РИСУЕМ РЫБ ===
    glDisable(GL_LIGHTING);
    for (int i = 0; i < activeFish; i++) {
        drawFish(fish[i], full_time);
    }
    if (lightning) glEnable(GL_LIGHTING);
    

    // ОРЁЛ:
    GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);

    struct Point3D { double x, y, z; };
    const Point3D P[4] = {
        { -20.0,  10.0, 4.0 },  
        {  20.0, -10.0, 2.0 },
        {  20.0,  40.0, 3.0 },
        { -20.0,  20.0, 1.0 }
    };

    // Параметр траектории
    double t = fmod(full_time * 0.04, 1.0);
    double u = 1.0 - t;

    // Позиция на кривой Безье
    Point3D pos;
    pos.x = u * u * u * P[0].x + 3 * u * u * t * P[1].x + 3 * u * t * t * P[2].x + t * t * t * P[3].x;
    pos.y = u * u * u * P[0].y + 3 * u * u * t * P[1].y + 3 * u * t * t * P[2].y + t * t * t * P[3].y;
    pos.z = u * u * u * P[0].z + 3 * u * u * t * P[1].z + 3 * u * t * t * P[2].z + t * t * t * P[3].z;

    // Направление в плоскости XY
    Point3D dir;
    dir.x = 3 * u * u * (P[1].x - P[0].x) + 6 * u * t * (P[2].x - P[1].x) + 3 * t * t * (P[3].x - P[2].x);
    dir.y = 3 * u * u * (P[1].y - P[0].y) + 6 * u * t * (P[2].y - P[1].y) + 3 * t * t * (P[3].y - P[2].y);

    // Угол поворота
    double yaw = atan2(dir.y, dir.x) * 180.0 / 3.14159265;

    // === ОТРИСОВКА ===
    simple_texture_sh.UseShader();
    location = glGetUniformLocationARB(simple_texture_sh.program, "tex");
    glUniform1iARB(location, 0);
    glActiveTexture(GL_TEXTURE0);
    berd_tex.Bind();

    glPushMatrix();
    glTranslated(pos.x, pos.y, pos.z);

    // Поворот по направлению полёта
    glRotated(yaw, 0, 0, 1);

    glRotated(90, 0, 0, 1);
    glRotated(180, 0, 0, 1);
    glRotated(180, 0, 1, 0);
    glScaled(0.6, 0.6, 0.6);
    glRotated(-90, 1, 0, 0);

    berd.Draw();
    glPopMatrix();

    if (lightingWasEnabled) glEnable(GL_LIGHTING);
 
    // Отрисовка пингвина с учётом позиции и поворота
    simple_texture_sh.UseShader();
    location = glGetUniformLocationARB(simple_texture_sh.program, "tex");
    glUniform1iARB(location, 0);
    glActiveTexture(GL_TEXTURE0);
    penguinTex.Bind();

    glPushMatrix();
    // Позиция пингвина
    glTranslated(penguinPos.x, penguinPos.y, penguinPos.z);
    // Поворот пингвина
    glRotated(180, 0, 1, 0);
    // Масштаб
    glScaled(0.7, 0.7, 0.7);
    glRotated(-90, 1, 0, 0);
    glRotated(penguinRotation, 0, 1, 0);
    penguinModel.Draw();
    glPopMatrix();

    //===============================================

    // Сбрасываем все трансформации
    glLoadIdentity();
    camera.SetUpCamera();
    Shader::DontUseShaders();
    // Рисуем источник света
    light.DrawLightGizmo();

    //================Сообщение в верхнем левом углу=======================
    glActiveTexture(GL_TEXTURE0);
    // Переключаемся на матрицу проекции
    glMatrixMode(GL_PROJECTION);
    // Сохраняем текущую матрицу проекции с перспективным преобразованием
    glPushMatrix();
    // Загружаем единичную матрицу в матрицу проекции
    glLoadIdentity();

    // Устанавливаем матрицу параллельной проекции
    glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

    // Переключаемся на матрицу MODELVIEW
    glMatrixMode(GL_MODELVIEW);
    // Сохраняем матрицу
    glPushMatrix();
    // Сбрасываем все трансформации и настройки камеры загрузкой единичной матрицы
    glLoadIdentity();

    // Нарисованное тут находится в 2D системе координат
    // Нижний левый угол окна - точка (0,0)
    // Верхний правый угол (ширина_окна - 1, высота_окна - 1)

    std::wstringstream ss;
    ss << std::fixed << std::setprecision(3) << " T - " << (texturing ? L"[вкл]выкл" : L"вкл[выкл]") << L" текстур\n"
        << " L - " << (lightning ? L"[вкл]выкл" : L"вкл[выкл]") << L" освещение\n"
        << L" C - " << (cameraFollowMode ? L"[авто]ручн" : L"авто[ручн]") << L" камера\n"
        << L" F - переместить свет в позицию камеры\n"
        << L" G - двигать свет по горизонтали\n"
        << L" G+ЛКМ - двигать свет по вертикали\n"
        << L" W - вперед   A - назад\n"
        << L" S - влево   D - вправо\n"
        << L" УРОВЕНЬ: " << std::setprecision(2) << currentLevel << L"/3                \n"
        << L" Рыбки: " << std::setw(2) << collectedFish << L" /" << std::setw(2) << activeFish << L"          \n";

    
    text.setPosition(10, gl.getHeight() - 30 - 150);
    text.setText(ss.str().c_str());
    text.Draw();
    
    if (gameState == GAME_OVER) {
        drawGameOver();
    }

    if (gameState == WIN) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, gl.getWidth(), 0, gl.getHeight(), -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        std::wstringstream winMsg;
        winMsg << L"\n\n\n\n╔════════════════════════════╗\n"
            << L"║         ПОБЕДА!            ║\n"
            << L"║   Пингвин сыт и доволен!   ║\n"
            << L"╠════════════════════════════╣\n"
            << L"║ Вы прошли все 3 уровня!    ║\n"
            << L"║ [ENTER] — сыграть еще раз  ║\n"
            << L"╚════════════════════════════╝";

        text.setSize(250, 250);
        text.setPosition(gl.getWidth() / 2 - 250, gl.getHeight() / 2 - 80);
        text.setText(winMsg.str().c_str());
        text.Draw();
        text.setSize(310, 180);

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }


    // Восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
