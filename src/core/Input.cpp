#include "Input.hpp"
#include "Window.hpp"
#include "Time.hpp"

namespace Lca
{
    namespace Input
    {

        void updateKey(Key& key, int glfwKey)
        {
            key.down = (glfwGetKey(Core::pGLFWwindow, glfwKey) == GLFW_PRESS);
            key.pressed = (key.down && key.lastPressed > keyPressTime);
            key.lastPressed = (key.lastPressed + Time::deltaTime) * !key.pressed;
        }

        void updateKeys()
        {
            // Alphabet keys
            updateKey(keyA, GLFW_KEY_A);
            updateKey(keyB, GLFW_KEY_B);
            updateKey(keyC, GLFW_KEY_C);
            updateKey(keyD, GLFW_KEY_D);
            updateKey(keyE, GLFW_KEY_E);
            updateKey(keyF, GLFW_KEY_F);
            updateKey(keyG, GLFW_KEY_G);
            updateKey(keyH, GLFW_KEY_H);
            updateKey(keyI, GLFW_KEY_I);
            updateKey(keyJ, GLFW_KEY_J);
            updateKey(keyK, GLFW_KEY_K);
            updateKey(keyL, GLFW_KEY_L);
            updateKey(keyM, GLFW_KEY_M);
            updateKey(keyN, GLFW_KEY_N);
            updateKey(keyO, GLFW_KEY_O);
            updateKey(keyP, GLFW_KEY_P);
            updateKey(keyQ, GLFW_KEY_Q);
            updateKey(keyR, GLFW_KEY_R);
            updateKey(keyS, GLFW_KEY_S);
            updateKey(keyT, GLFW_KEY_T);
            updateKey(keyU, GLFW_KEY_U);
            updateKey(keyV, GLFW_KEY_V);
            updateKey(keyW, GLFW_KEY_W);
            updateKey(keyX, GLFW_KEY_X);
            updateKey(keyY, GLFW_KEY_Y);
            updateKey(keyZ, GLFW_KEY_Z);

            // Numeric keys
            updateKey(key0, GLFW_KEY_0);
            updateKey(key1, GLFW_KEY_1);
            updateKey(key2, GLFW_KEY_2);
            updateKey(key3, GLFW_KEY_3);
            updateKey(key4, GLFW_KEY_4);
            updateKey(key5, GLFW_KEY_5);
            updateKey(key6, GLFW_KEY_6);
            updateKey(key7, GLFW_KEY_7);
            updateKey(key8, GLFW_KEY_8);
            updateKey(key9, GLFW_KEY_9);

            // Arrow keys
            updateKey(keyLeft, GLFW_KEY_LEFT);
            updateKey(keyRight, GLFW_KEY_RIGHT);
            updateKey(keyUp, GLFW_KEY_UP);
            updateKey(keyDown, GLFW_KEY_DOWN);

            // Special keys
            updateKey(keySpace, GLFW_KEY_SPACE);
            updateKey(keyEsc, GLFW_KEY_ESCAPE);
            updateKey(keyShift, GLFW_KEY_LEFT_SHIFT);
            updateKey(keyEnter, GLFW_KEY_ENTER);
            updateKey(keyBackspace, GLFW_KEY_BACKSPACE);
            updateKey(keyTab, GLFW_KEY_TAB);
        }

        void updateButtons()
        {
            buttonLeft.down = (glfwGetMouseButton(Core::pGLFWwindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            buttonLeft.pressed = (buttonLeft.down && buttonLeft.lastPressed > buttonPressTime);
            buttonLeft.lastPressed = (buttonLeft.lastPressed + Time::deltaTime) * !buttonLeft.pressed;

            buttonRight.down = (glfwGetMouseButton(Core::pGLFWwindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
            buttonRight.pressed = (buttonRight.down && buttonRight.lastPressed > buttonPressTime);
            buttonRight.lastPressed = (buttonRight.lastPressed + Time::deltaTime) * !buttonRight.pressed;

        }

        void createCursor()
        {
            glfwSetCursorPos
            (Core::pGLFWwindow,
            Core::windowWidth/2.f, Core::windowHeight);

            cursor.position = glm::vec2(Core::windowWidth/2.f, Core::windowHeight/2.f);

            cursor.deltaPosition = glm::vec2(0.f,0.f);
        }

        void updateCursor()
        {
            double posX, posY;
            glfwGetCursorPos(Core::pGLFWwindow, &posX, &posY);

            glm::vec2 lastPosition = cursor.position;

            cursor.position = 
            glm::vec2
            (static_cast<float>(posX),
            static_cast<float>(posY));

            cursor.deltaPosition =  cursor.position - lastPosition;

            std::cout << "Cursor Position: (" << cursor.position.x-(Core::windowWidth/2.f) << ", " << cursor.position.y - (Core::windowHeight/2.f) << ")\n";
        }

        void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
        {
            scroll.scroll += static_cast<float>(yoffset);
            scroll.deltaScroll = static_cast<float>(yoffset);
        }

        void createScroll()
        {
            scroll.scroll = 0.f;
            scroll.deltaScroll = 0.f;

            glfwSetScrollCallback(Core::pGLFWwindow, scrollCallback);
        }

        void createInput()
        {
            createCursor();
            createScroll();
        }

        void updateInput()
        {
            updateKeys();
            updateButtons();
            updateCursor();
        }
    }
}