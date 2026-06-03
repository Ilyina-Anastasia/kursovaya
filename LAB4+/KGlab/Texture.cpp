#include "Texture.h"

#include <windows.h>
#include <GL/gl.h>


// Библиотека для разгрузки изображений
// https://github.com/nothings/stb
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::~Texture()
{
    if (texId != 0)
        glDeleteTextures(1, &texId);
}

void Texture::LoadTexture(const std::string& texture_file_name)
{
    if (texId != 0)
        glDeleteTextures(1, &texId);

    // Генерируем ID текстуры
    glGenTextures(1, &texId);

    int curent_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &curent_texture);

    // Делаем текущую текстуру активной
    glBindTexture(GL_TEXTURE_2D, texId);

    int x, y, n;

    // Загружаем картинку
    // см. #include "stb_image.h"
    unsigned char* data = stbi_load(texture_file_name.c_str(), &x, &y, &n, 4);
    // x - ширина изображения
    // y - высота изображения
    // n - количество каналов
    // 4 - нужное нам количество каналов
    // Пиксели будут хранится в памяти [R-G-B-A]-[R-G-B-A]-[.....
    //  по 4 байта на пиксель - по байту на канал
    // Пустые каналы будут равны 255

    // Картинка хранится в памяти перевернутой,
    // так как ее начало в левом верхнем углу;
    // поэтому мы ее переворачиваем -
    // меняем первую строку с последней,
    // вторую с предпоследней, и.т.д.
    unsigned char* _tmp = new unsigned char[x * 4];
    for (int i = 0; i < y / 2; ++i)
    {
        std::memcpy(_tmp, data + i * x * 4, x * 4);                       // переносим строку i -> tmp
        std::memcpy(data + i * x * 4, data + (y - 1 - i) * x * 4, x * 4); // (y-1-i)я строка -> iя строка
        std::memcpy(data + (y - 1 - i) * x * 4, _tmp, x * 4);             // tmp -> (y-1-i)я строка
    }
    delete[] _tmp;

    // Загрузка изображения в видеопамять
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Выгрузка изображения из опперативной памяти
    stbi_image_free(data);

    // Настройка режима наложения текстур
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    // GL_REPLACE -- полная замена политога текстурой
    // Настройка тайлинга
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Настройка фильтрации
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //======================================================

    glBindTexture(GL_TEXTURE_2D, curent_texture);
}

void Texture::Bind()
{
    glBindTexture(GL_TEXTURE_2D, texId);
}
