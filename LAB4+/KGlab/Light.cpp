#include "Light.h"
#include "MyOGL.h"
#include "Vector3.h"

#include <algorithm>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <tuple>
#include <windows.h>

extern OpenGL gl;

std::tuple<double, double, double, double, double, double> getLookRay(int wndX, int wndY)
{
    GLint viewport[4];       // Параметры viewport-a.
    GLdouble projection[16]; // Матрица проекции.
    GLdouble modelview[16];  // Видовая матрица.
    GLdouble wx, wy, wz;     // Возвращаемые мировые координаты.

    glGetIntegerv(GL_VIEWPORT, viewport);           // Узнаём параметры viewport-a.
    glGetDoublev(GL_PROJECTION_MATRIX, projection); // Узнаём матрицу проекции.
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);   // Узнаём видовую матрицу.

    // Переводим оконные координаты курсора в систему координат viewport-a.

    double originX, originY, originZ;          // Точка в 3D мире под мышью
    double directionX, directionY, directionZ; // Направление клика

    // Обратное проекцирование 2d->3d (wndX, wndY, 0) -> (wx,wy,wz)   0 - глубина внутрь экрана
    gluUnProject(wndX, wndY, 0, modelview, projection, viewport, &wx, &wy, &wz);
    originX = wx;
    originY = wy;
    originZ = wz;

    // Обратное проекцирование 2d->3d (wndX, wndY, 1) -> (wx,wy,wz)   1 - глубина внутрь экрана
    gluUnProject(wndX, wndY, 1, modelview, projection, viewport, &wx, &wy, &wz);
    directionX = wx;
    directionY = wy;
    directionZ = wz;

    directionX -= originX;
    directionY -= originY;
    directionZ -= originZ;

    double length = sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);

    directionX /= length;
    directionY /= length;
    directionZ /= length;

    return {originX, originY, originZ, directionX, directionY, directionZ};
}

void Light::SetPosition(double x, double y, double z)
{
    posX = x;
    posY = y;
    posZ = z;
}

void Light::StartDrug(OpenGL* sender, KeyEventArg arg)
{
    if (arg.key == 0x47) // клавиша G
    {
        drag = true;
    }

    if (arg.key == 0x46) // клавиша F
    {
        from_camera = true;
    }
}

void Light::StopDrug(OpenGL* sender, KeyEventArg arg)
{
    if (arg.key == 0x47) // клавиша G
    {
        drag = false;
    }
    if (arg.key == 0x46) // клавиша F
    {
        from_camera = false;
    }
}

void Light::MoveLight(OpenGL* sender, MouseEventArg arg)
{
    // Двигаем свет по плоскости
    if (drag)
    {
        int _x = arg.x;
        int _y = gl.getHeight() - arg.y;

        auto [oX, oY, oZ, dX, dY, dZ] = getLookRay(_x, _y);

        if (!OpenGL::isKeyPressed(VK_LBUTTON)) // Если не нажата левая кнопка мыши
        {
            double z = posZ;

            double k = 0, x = 0, y = 0;
            if (dZ == 0)
                k = 0;
            else
                k = (z - oZ) / dZ;

            x = k * dX + oX;
            y = k * dY + oY;

            if (x * x + y * y > 2500) // Ограничение максимальной дистанции
                return;

            posX = x;
            posY = y;
            posZ = z;
        }
        else // Если нажата ЛКМ
        {
            Vector3 o{oX, oY, oZ};
            Vector3 d{dX, dY, dZ};
            Vector3 z{0, 0, 1};

            Vector3 _top = d ^ Vector3(0, 0, 1) ^ d;

            // Уравнение плоскости Ax+By+Cz+D=0  _top = (A, B, C)

            // Ищем D
            double D = -_top.x() * oX - _top.y() * oY - _top.z() * oZ;

            // Ищем новую координату Z света
            if (_top.z() == 0)
                posZ = 0;
            else
                posZ = std::clamp(-(_top.x() * posX + _top.y() * posY + D) / _top.z(), -20.0, 20.0);
        }
    }
}

void Light::SetUpLight()
{
    // Характеристики излучаемого света

    // Фоновое освещение (рассеянный свет)
    GLfloat lamb[] = {0.2, 0.2, 0.2, 0};
    // Диффузная составляющая света
    GLfloat ldif[] = {0.7, 0.7, 0.7, 0};
    // Зеркально отражаемая составляющая света
    GLfloat lspec[] = {1.0, 1.0, 1.0, 0};
    // Координаты
    GLfloat lposition[] = {posX, posY, posZ, 1.};

    // Сообщаем эти значения OpenGL
    glLightfv(GL_LIGHT0, GL_POSITION, lposition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lspec);
    glEnable(GL_LIGHT0);
}

void Light::DrawLightGizmo()
{
    // Рисуем точку, откуда идет свет

    // Устанавливаем размер точки
    GLfloat pointSize;
    glGetFloatv(GL_POINT_SIZE, &pointSize);
    glPointSize(10);

    // Отключаем тест глубины, чтобы точка рисовалась сквозь все
    glDisable(GL_DEPTH_TEST);

    // Отключаем свет и текстуры
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // Рисуем точку
    glBegin(GL_POINTS);
    glColor3d(1, 0.7, 0.1);
    glVertex3d(posX, posY, posZ);
    glEnd();

    // Восстанавливаем предыдущий размер точки
    glPointSize(pointSize);

    // Если нажата G - рисуем линии осей от света
    if (!drag)
        return;

    GLfloat lineWidth;
    glGetFloatv(GL_LINE_WIDTH, &lineWidth);

    glLineWidth(3.0);

    glBegin(GL_LINES);
    glColor3d(0, 0, 0.8);
    glVertex3d(posX, posY, posZ);
    glVertex3d(posX, posY, 0);

    glColor3d(0.8, 0, 0);
    glVertex3d(posX - 1, posY, 0);
    glVertex3d(posX + 1, posY, 0);

    glColor3d(0, 0.8, 0);
    glVertex3d(posX, posY - 1, 0);
    glVertex3d(posX, posY + 1, 0);

    glEnd();

    glLineWidth(lineWidth);
}
