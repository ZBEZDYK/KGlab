#include "Render.h"
#include "GUItextRectangle.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>

#ifdef _DEBUG
#include <Debugapi.h>
struct debug_print
{
    template <class C> debug_print& operator<<(const C& a)
    {
        OutputDebugStringA((std::stringstream() << a).str().c_str());
        return *this;
    }
} debout;
#else
struct debug_print
{
    template <class C> debug_print& operator<<(const C& a)
    {
        return *this;
    }
} debout;
#endif

// Библиотека для разгрузки изображений
// https://github.com/nothings/stb
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;

bool texturing = true;
bool lightning = true;
bool alpha = false;

// Переключение режимов освещения, текстурирования, альфа-наложения
void switchModes(OpenGL* sender, KeyEventArg arg)
{
    // Конвертируем код клавиши в букву
    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

    switch (key)
    {
    case 'L':
        lightning = !lightning;
        break;
    case 'T':
        texturing = !texturing;
        break;
    case 'A':
        alpha = !alpha;
        break;
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

// Функция для расчета нормали по трем точкам
void CalculateNormal(double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3,
    double& nx, double& ny, double& nz)
{
    double ax = x2 - x1;
    double ay = y2 - y1;
    double az = z2 - z1;

    double bx = x3 - x1;
    double by = y3 - y1;
    double bz = z3 - z1;

    nx = ay * bz - az * by;
    ny = az * bx - ax * bz;
    nz = ax * by - ay * bx;

    double len = sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0) {
        nx /= len; ny /= len; nz /= len;
    }
}

// Выполняется один раз перед первым рендером
void initRender()
{
    //==============НАСТРОЙКА ТЕКСТУР================
    // 4 байта на хранение пикселя
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // Генерируем ID текстуры
    glGenTextures(1, &texId);

    // Делаем текущую текстуру активной
    glBindTexture(GL_TEXTURE_2D, texId);

    int x, y, n;

    // Загружаем картинку
    // см. #include "stb_image.h"
    unsigned char* data = stbi_load("texture.png", &x, &y, &n, 4);

    if (data)
    {
        // Картинка хранится в памяти перевернутой
        unsigned char* _tmp = new unsigned char[x * 4];
        for (int i = 0; i < y / 2; ++i)
        {
            std::memcpy(_tmp, data + i * x * 4, x * 4);
            std::memcpy(data + i * x * 4, data + (y - 1 - i) * x * 4, x * 4);
            std::memcpy(data + (y - 1 - i) * x * 4, _tmp, x * 4);
        }
        delete[] _tmp;

        // Загрузка изображения в видеопамять
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    // Настройка режима наложения текстур
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    // Настройка тайлинга
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Настройка фильтрации
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //======================================================

    //================НАСТРОЙКА КАМЕРЫ======================
    camera.caclulateCameraPos();

    // Привязываем камеру к событиям "движка"
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
    gl.KeyDownEvent.reaction(switchModes);
    text.setSize(512, 220);
    //========================================================

    camera.setPosition(2, 1.5, 1.5);
}

void Render(double delta_time)
{
    glEnable(GL_DEPTH_TEST);

    // Настройка камеры и света
    if (gl.isKeyPressed('F')) // если нажата F - свет из камеры
    {
        light.SetPosition(camera.x(), camera.y(), camera.z());
    }
    camera.SetUpCamera();
    light.SetUpLight();

    // Рисуем оси
    gl.DrawAxes();

    //программировать тут

    // Настройка освещения и материала
    if (lightning)
        glEnable(GL_LIGHTING);
    else
        glDisable(GL_LIGHTING);

    if (texturing)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (alpha)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    //=============НАСТРОЙКА МАТЕРИАЛА==============
    float amb[] = { 0.3, 0.3, 0.3, 1. };
    float dif[] = { 0.8, 0.8, 0.8, 1. };
    float spec[] = { 1.0, 1.0, 1.0, 1. };
    float sh = 0.5f * 256;

    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, sh);

    glShadeModel(GL_SMOOTH);

    // ========== ПРИЗМА ==========
    double x[8] = { 0, 7, 8, 1, -3, -2, -7, -4 };
    double y[8] = { 1, 5, -4, -2, -7, -1, 1, 6 };
    double z_bottom = 0;
    double z_top = 3;

    double colors[8][3] = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0},
        {1, 0, 1}, {0, 1, 1}, {1, 0.5, 0}, {0.5, 0, 1}
    };

    // Боковые грани - нормали
    for (int i = 0; i < 8; i++) {
        int next = (i + 1) % 8;

        // Векторы грани
        double ax = x[next] - x[i];
        double ay = y[next] - y[i];
        double az = 0;

        double bx = 0;
        double by = 0;
        double bz = z_top - z_bottom;

        // Нормаль через векторное произведение
        double nx = ay * bz - az * by;
        double ny = az * bx - ax * bz;
        double nz = ax * by - ay * bx;

        // Нормализация
        double len = sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0) {
            nx /= len; ny /= len; nz /= len;
        }

        double centerX = (x[i] + x[next]) / 2.0;
        double centerY = (y[i] + y[next]) / 2.0;
        double centerZ = (z_bottom + z_top) / 2.0;

        // Вектор из центра призмы (0,0,1.5) в центр грани
        double toCenterX = centerX - 0;
        double toCenterY = centerY - 0;
        double toCenterZ = centerZ - 1.5;

        // Скалярное произведение
        double dot = nx * toCenterX + ny * toCenterY + nz * toCenterZ;

        if (dot < 0) {
            nx = -nx;
            ny = -ny;
            nz = -nz;
        }

        glBegin(GL_QUADS);
        glNormal3d(nx, ny, nz);
        glColor3d(colors[i][0], colors[i][1], colors[i][2]);
        glVertex3d(x[i], y[i], z_bottom);
        glVertex3d(x[next], y[next], z_bottom);
        glVertex3d(x[next], y[next], z_top);
        glVertex3d(x[i], y[i], z_top);
        glEnd();
    }

    // Дно - нормаль (вниз)
    glNormal3d(0, 0, -1);
    glColor3d(0.5, 0.5, 0.5);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(0, 0, z_bottom);
    for (int i = 0; i <= 8; i++) {
        glVertex3d(x[i % 8], y[i % 8], z_bottom);
    }
    glEnd();

    // Крышка с текстурой - нормаль (вверх)
    glNormal3d(0, 0, 1);
    glBindTexture(GL_TEXTURE_2D, texId);

    if (alpha)
        glColor4d(1, 1, 1, 0.7);
    else
        glColor4d(1, 1, 1, 1);

    double minX = -7, maxX = 8, minY = -7, maxY = 6;
    double centerU = (0 - minX) / (maxX - minX);
    double centerV = (0 - minY) / (maxY - minY);

    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2d(centerU, centerV);
    glVertex3d(0, 0, z_top);

    for (int i = 0; i <= 8; i++) {
        int idx = i % 8;
        double u = (x[idx] - minX) / (maxX - minX);
        double v = (y[idx] - minY) / (maxY - minY);
        glTexCoord2d(u, v);
        glVertex3d(x[idx], y[idx], z_top);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glColor4d(1, 1, 1, 1);

    // Нормали боковых граней
    glColor3d(1, 1, 0);
    glLineWidth(2);

    for (int i = 0; i < 8; i++) {
        int next = (i + 1) % 8;

        // Центр грани
        double centerX = (x[i] + x[next]) / 2.0;
        double centerY = (y[i] + y[next]) / 2.0;
        double centerZ = (z_bottom + z_top) / 2.0;

        double ax = x[next] - x[i];
        double ay = y[next] - y[i];
        double az = 0;
        double bx = 0, by = 0, bz = z_top - z_bottom;

        double nx = ay * bz - az * by;
        double ny = az * bx - ax * bz;
        double nz = ax * by - ay * bx;

        double len = sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0) {
            nx /= len; ny /= len; nz /= len;
        }

        double toCenterX = centerX - 0;
        double toCenterY = centerY - 0;
        double toCenterZ = centerZ - 1.5;
        double dot = nx * toCenterX + ny * toCenterY + nz * toCenterZ;
        if (dot < 0) {
            nx = -nx;
            ny = -ny;
            nz = -nz;
        }

        glBegin(GL_LINES);
        glVertex3d(centerX, centerY, centerZ);
        glVertex3d(centerX + nx * 0.8, centerY + ny * 0.8, centerZ + nz * 0.8);
        glEnd();

        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex3d(centerX + nx * 0.8, centerY + ny * 0.8, centerZ + nz * 0.8);
        glEnd();
    }

    // Нормаль дна (вниз)
    glColor3d(0, 1, 1);
    double centerBottomX = 0, centerBottomY = 0, centerBottomZ = z_bottom;
    glBegin(GL_LINES);
    glVertex3d(centerBottomX, centerBottomY, centerBottomZ);
    glVertex3d(centerBottomX, centerBottomY, centerBottomZ - 0.8);
    glEnd();
    glBegin(GL_POINTS);
    glVertex3d(centerBottomX, centerBottomY, centerBottomZ - 0.8);
    glEnd();

    // Нормаль крышки (вверх)
    glColor3d(1, 0, 1);
    double centerTopX = 0, centerTopY = 0, centerTopZ = z_top;
    glBegin(GL_LINES);
    glVertex3d(centerTopX, centerTopY, centerTopZ);
    glVertex3d(centerTopX, centerTopY, centerTopZ + 0.8);
    glEnd();
    glBegin(GL_POINTS);
    glVertex3d(centerTopX, centerTopY, centerTopZ + 0.8);
    glEnd();


    // Рисуем источник света
    light.DrawLightGizmo();

    //================Сообщение в верхнем левом углу=======================
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    std::wstringstream ss;
    ss << std::fixed << std::setprecision(3) << "T - " << (texturing ? L"[вкл]выкл" : L"вкл[выкл]") << L" текстур\n"
        << "L - " << (lightning ? L"[вкл]выкл" : L"вкл[выкл]") << L" освещение\n"
        << "A - " << (alpha ? L"[вкл]выкл" : L"вкл[выкл]") << L" альфа-наложение\n"
        << L"F - переместить свет в позицию камеры\n"
        << L"G - двигать свет по горизонтали\n"
        << L"G+ЛКМ - двигать свет по вертикали\n"
        << L"Координаты света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7)
        << light.z() << ")\n"
        << L"Координаты камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << ","
        << std::setw(7) << camera.z() << ")\n"
        << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ", fi1=" << std::setw(7) << camera.fi1()
        << ", fi2=" << std::setw(7) << camera.fi2() << '\n'
        << L"delta_time: " << std::setprecision(5) << delta_time;

    text.setPosition(10, gl.getHeight() - 10 - 220);
    text.setText(ss.str().c_str());
    text.Draw();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}