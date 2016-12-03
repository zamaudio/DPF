/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2016 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <GLFW/glfw3.h>

#include "../Base.hpp"

#include "ApplicationPrivateData.hpp"
#include "WidgetPrivateData.hpp"
#include "../StandaloneWindow.hpp"
#include "../../distrho/extra/String.hpp"

#define FOR_EACH_WIDGET(it) \
  for (std::list<Widget*>::iterator it = fWidgets.begin(); it != fWidgets.end(); ++it)

#define FOR_EACH_WIDGET_INV(rit) \
  for (std::list<Widget*>::reverse_iterator rit = fWidgets.rbegin(); rit != fWidgets.rend(); ++rit)

#ifdef DEBUG
# define DBG(msg)  std::fprintf(stderr, "%s", msg);
# define DBGp(...) std::fprintf(stderr, __VA_ARGS__);
# define DBGF      std::fflush(stderr);
#else
# define DBG(msg)
# define DBGp(...)
# define DBGF
#endif

START_NAMESPACE_DGL

// -----------------------------------------------------------------------
// Window Private

struct Window::PrivateData {
    PrivateData(Application& app, Window* const self)
        : fApp(app),
          fSelf(self),
          fView(nullptr),
          fFirstInit(true),
          fVisible(false),
          fResizable(true),
          fUsingEmbed(false),
          fWidth(1),
          fHeight(1),
          fTitle(nullptr),
          fWidgets(),
          fModal(),
          monitor(nullptr)
    {
        DBG("Creating window without parent..."); DBGF;
        init();
    }

    PrivateData(Application& app, Window* const self, Window& parent)
        : fApp(app),
          fSelf(self),
          fView(nullptr),
          fFirstInit(true),
          fVisible(false),
          fResizable(true),
          fUsingEmbed(false),
          fWidth(1),
          fHeight(1),
          fTitle(nullptr),
          fWidgets(),
          fModal(parent.pData),
          monitor(nullptr)
    {
        DBG("Creating window with parent..."); DBGF;
        init();
    }

    PrivateData(Application& app, Window* const self, const intptr_t parentId)
        : fApp(app),
          fSelf(self),
          fView(nullptr),
          fFirstInit(true),
          fVisible(parentId != 0),
          fResizable(parentId == 0),
          fUsingEmbed(parentId != 0),
          fWidth(1),
          fHeight(1),
          fTitle(nullptr),
          fWidgets(),
          fModal()
    {
        if (fUsingEmbed)
        {
            DBG("Creating embedded window..."); DBGF;
            //puglInitWindowParent(fView, parentId);
        }
        else
        {
            DBG("Creating window without parent..."); DBGF;
        }

        init();

        if (fUsingEmbed)
        {
            DBG("NOTE: Embed window is always visible and non-resizable\n");
            glfwShowWindow(fView);
            fApp.pData->oneShown();
            fFirstInit = false;
        }
    }

    void init()
    {
        if (fSelf == nullptr )//|| fView == nullptr)
        {
            DBG("Failed!\n");
            return;
        }

        if (!glfwInit())
        {
            DBG("Failed!\n");
            exit(1);
        }

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        fView = glfwCreateWindow(static_cast<int>(fWidth), static_cast<int>(fHeight), "title", nullptr, nullptr);

	// XXX do callbacks latter
#if 0
        puglSetSpecialFunc(fView, onSpecialCallback);
        puglSetFileSelectedFunc(fView, fileBrowserSelectedCallback);
#endif
	glfwSetWindowUserPointer(fView, this);
	glfwSetWindowRefreshCallback(fView, onDisplayCallback);
        glfwSetWindowSizeCallback(fView, onReshapeCallback);
	glfwSetKeyCallback(fView, onKeyboardCallback);
	glfwSetMouseButtonCallback(fView, onMouseCallback);
	glfwSetScrollCallback(fView, onScrollCallback);
        glfwSetCursorPosCallback(fView, onMotionCallback);
	glfwSetWindowCloseCallback(fView, onCloseCallback);

        glfwMakeContextCurrent(fView);

        fApp.pData->windows.push_back(fSelf);

        DBG("Success!\n");
    }

    ~PrivateData()
    {
        DBG("Destroying window..."); DBGF;

        if (fModal.enabled)
        {
            exec_fini();
            close();
        }

        fWidgets.clear();

        if (fUsingEmbed)
        {
            glfwHideWindow(fView);
            fApp.pData->oneHidden();
        }

        if (fSelf != nullptr)
        {
            fApp.pData->windows.remove(fSelf);
            fSelf = nullptr;
        }

        if (fView != nullptr)
        {
            glfwDestroyWindow(fView);
            fView = nullptr;
        }

        if (fTitle != nullptr)
        {
            std::free(fTitle);
            fTitle = nullptr;
        }

        monitor = nullptr;

        DBG("Success!\n");
    }

    // -------------------------------------------------------------------

    void close()
    {
        DBG("Window close\n");

        if (fUsingEmbed)
            return;

        setVisible(false);

        if (! fFirstInit)
        {
            fApp.pData->oneHidden();
            fFirstInit = true;
        }
    }

    void exec(const bool lockWait)
    {
        DBG("Window exec\n");
        exec_init();

        if (lockWait)
        {
            for (; fVisible && fModal.enabled;)
            {
                idle();
                d_msleep(10);
            }

            exec_fini();
        }
        else
        {
            idle();
        }
    }

    // -------------------------------------------------------------------

    void exec_init()
    {
        DBG("Window modal loop starting..."); DBGF;
        DISTRHO_SAFE_ASSERT_RETURN(fModal.parent != nullptr, setVisible(true));

        fModal.enabled = true;
        fModal.parent->fModal.childFocus = this;

        fModal.parent->setVisible(true);
        setVisible(true);

        DBG("Ok\n");
    }

    void exec_fini()
    {
        DBG("Window modal loop stopping..."); DBGF;
        fModal.enabled = false;

        if (fModal.parent != nullptr)
            fModal.parent->fModal.childFocus = nullptr;

        DBG("Ok\n");
    }

    // -------------------------------------------------------------------

    void focus()
    {
        DBG("Window focus\n");
	glfwFocusWindow(fView);
    }

    // -------------------------------------------------------------------

    void setVisible(const bool yesNo)
    {
        if (fVisible == yesNo)
        {
            DBG("Window setVisible matches current state, ignoring request\n");
            return;
        }
        if (fUsingEmbed)
        {
            DBG("Window setVisible cannot be called when embedded\n");
            return;
        }

        DBG("Window setVisible called\n");

        fVisible = yesNo;

        if (yesNo && fFirstInit)
            setSize(fWidth, fHeight, true);

        if (yesNo)
            glfwShowWindow(fView);
        else
            glfwHideWindow(fView);

        if (yesNo)
        {
            if (fFirstInit)
            {
                fApp.pData->oneShown();
                fFirstInit = false;
            }
        }
        else if (fModal.enabled)
            exec_fini();
    }

    // -------------------------------------------------------------------

    void setResizable(const bool yesNo)
    {
        if (fResizable == yesNo)
        {
            DBG("Window setResizable matches current state, ignoring request\n");
            return;
        }
        if (fUsingEmbed)
        {
            DBG("Window setResizable cannot be called when embedded\n");
            return;
        }

        DBG("Window setResizable called\n");

        fResizable = yesNo;

#if defined(DISTRHO_OS_MAC)
        const uint flags(yesNo ? (NSViewWidthSizable|NSViewHeightSizable) : 0x0);
        [mView setAutoresizingMask:flags];
#endif

        setSize(fWidth, fHeight, true);
    }

    // -------------------------------------------------------------------

    void setSize(uint width, uint height, const bool forced = false)
    {
        if (width <= 1 || height <= 1)
        {
            DBGp("Window setSize called with invalid value(s) %i %i, ignoring request\n", width, height);
            return;
        }

        if (fWidth == width && fHeight == height && ! forced)
        {
            DBGp("Window setSize matches current size, ignoring request (%i %i)\n", width, height);
            return;
        }

        fWidth  = width;
        fHeight = height;

        DBGp("Window setSize called %s, size %i %i, resizable %s\n", forced ? "(forced)" : "(not forced)", width, height, fResizable?"true":"false");

	glfwSetWindowSize(fView, width, height);

        //puglPostRedisplay(fView);
	glfwSwapBuffers(fView);
    }

    // -------------------------------------------------------------------

    const char* getTitle() const noexcept
    {
        static const char* const kFallback = "";

        return fTitle != nullptr ? fTitle : kFallback;
    }

    void setTitle(const char* const title)
    {
        DBGp("Window setTitle \"%s\"\n", title);

        if (fTitle != nullptr)
            std::free(fTitle);

        fTitle = strdup(title);

	glfwSetWindowTitle(fView, title);
    }

    void setTransientWinId(const uintptr_t)
    {
    }

    // -------------------------------------------------------------------

    void addWidget(Widget* const widget)
    {
        fWidgets.push_back(widget);
    }

    void removeWidget(Widget* const widget)
    {
        fWidgets.remove(widget);
    }

    void idle()
    {
	glfwPollEvents();
	glfwSwapBuffers(fView);

#ifdef DISTRHO_OS_MAC
        if (fNeedsIdle)
        {
            NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
            NSEvent* event;

            for (;;)
            {
                event = [NSApp
                         nextEventMatchingMask:NSAnyEventMask
                                     untilDate:[NSDate distantPast]
                                        inMode:NSDefaultRunLoopMode
                                       dequeue:YES];

                if (event == nil)
                    break;

                [NSApp sendEvent: event];
            }

            [pool release];
        }
#endif

        if (fModal.enabled && fModal.parent != nullptr)
            fModal.parent->idle();
    }

    // -------------------------------------------------------------------

#if 0 // XXX do callbacks later
    int onPuglSpecial(const bool press, const Key key)
    {
        DBGp("PUGL: onSpecial : %i %i\n", press, key);

        if (fModal.childFocus != nullptr)
        {
            fModal.childFocus->focus();
            return 0;
        }

        Widget::SpecialEvent ev;
        ev.press = press;
        ev.key   = key;
        ev.mod   = static_cast<Modifier>(puglGetModifiers(fView));
        ev.time  = puglGetEventTimestamp(fView);

        FOR_EACH_WIDGET_INV(rit)
        {
            Widget* const widget(*rit);

            if (widget->isVisible() && widget->onSpecial(ev))
                return 0;
        }

        return 1;
    }
#endif // XXX do callbacks later

    void onPuglMotion(const int x, const int y)
    {
        DBGp("PUGL: onMotion : %i %i\n", x, y);

        if (fModal.childFocus != nullptr)
            return;

        Widget::MotionEvent ev;
        //ev.mod  = static_cast<Modifier>(mods);
        ev.time = (uint32_t)(glfwGetTime() * 1000.);

        FOR_EACH_WIDGET_INV(rit)
        {
            Widget* const widget(*rit);

            ev.pos = Point<int>(x-widget->getAbsoluteX(), y-widget->getAbsoluteY());

            if (widget->isVisible() && widget->onMotion(ev))
                break;
        }
    }

    void onPuglReshape(const int width, const int height)
    {
        DBGp("PUGL: onReshape : %i %i\n", width, height);

        if (width <= 1 && height <= 1)
            return;

        fWidth  = static_cast<uint>(width);
        fHeight = static_cast<uint>(height);

        fSelf->onReshape(fWidth, fHeight);

        FOR_EACH_WIDGET(it)
        {
            Widget* const widget(*it);

            if (widget->pData->needsFullViewport)
                widget->setSize(fWidth, fHeight);
        }
    }

    void onPuglDisplay()
    {
        fSelf->onDisplayBefore();

        FOR_EACH_WIDGET(it)
        {
            Widget* const widget(*it);
            widget->pData->display(fWidth, fHeight);
        }

        fSelf->onDisplayAfter();
    }

    void onPuglKeyboard(const bool press, const uint key, const uint mods)
    //static void onKeyboardCallback(GLFWwindow * view, int key, int scancode, int press, int mods)
    {
        DBGp("GLFW: onKeyboard : %i %i\n", press, key);

        if (fModal.childFocus != nullptr)
        {
            fModal.childFocus->focus();
            return;
        }

        Widget::KeyboardEvent ev;
        ev.press = press;
        ev.key  = key;
        ev.mod  = static_cast<Modifier>(mods);
        ev.time = (uint32_t)(glfwGetTime() * 1000.);

        FOR_EACH_WIDGET_INV(rit)
        {
            Widget* const widget(*rit);

            if (widget->isVisible() && widget->onKeyboard(ev))
                return;
        }
    }

    void onPuglMouse(const int button, const bool press, const int x, const int y, const int mods)
    //static void onMouseCallback(GLFWwindow * view, int button, int press, int mods)
    {
        DBGp("GLFW: onMouse : %i %i %i %i\n", button, press, x, y);

        if (fModal.childFocus != nullptr)
            return fModal.childFocus->focus();

        Widget::MouseEvent ev;
        ev.button = button;
        ev.press  = press;
        ev.mod    = static_cast<Modifier>(mods);
        ev.time = (uint32_t)(glfwGetTime() * 1000.);

        FOR_EACH_WIDGET_INV(rit)
        {
            Widget* const widget(*rit);

            ev.pos = Point<int>(x-widget->getAbsoluteX(), y-widget->getAbsoluteY());

            if (widget->isVisible() && widget->onMouse(ev))
                break;
        }
    }

    void onPuglScroll(const int x, const int y, const float dx, const float dy)
    //static void onScrollCallback(GLFWwindow* view, double dx, double dy)
    {
        DBGp("GLFW: onScroll : %i %i %f %f\n", x, y, dx, dy);

        if (fModal.childFocus != nullptr)
            return;

        Widget::ScrollEvent ev;
        ev.delta = Point<float>(dx, dy);
        //ev.mod   = static_cast<Modifier>(mods);
        ev.time = (uint32_t)(glfwGetTime() * 1000.);

        FOR_EACH_WIDGET_INV(rit)
        {
            Widget* const widget(*rit);

            ev.pos = Point<int>(x-widget->getAbsoluteX(), y-widget->getAbsoluteY());

            if (widget->isVisible() && widget->onScroll(ev))
                break;
        }
    }

    void onPuglClose()
    //static void onCloseCallback(GLFWwindow * view)
    {
        DBG("GLFW: onClose\n");

        if (fModal.enabled)
            exec_fini();

        fSelf->onClose();

        if (fModal.childFocus != nullptr)
            fModal.childFocus->fSelf->onClose();

        close();
    }

    // -------------------------------------------------------------------

    Application& fApp;
    Window*      fSelf;
    GLFWwindow*  fView;

    bool fFirstInit;
    bool fVisible;
    bool fResizable;
    bool fUsingEmbed;
    uint fWidth;
    uint fHeight;
    char* fTitle;
    std::list<Widget*> fWidgets;

    struct Modal {
        bool enabled;
        PrivateData* parent;
        PrivateData* childFocus;

        Modal()
            : enabled(false),
              parent(nullptr),
              childFocus(nullptr) {}

        Modal(PrivateData* const p)
            : enabled(false),
              parent(p),
              childFocus(nullptr) {}

        ~Modal()
        {
            DISTRHO_SAFE_ASSERT(! enabled);
            DISTRHO_SAFE_ASSERT(childFocus == nullptr);
        }

        DISTRHO_DECLARE_NON_COPY_STRUCT(Modal)
    } fModal;

    GLFWmonitor * monitor;

    // -------------------------------------------------------------------
    // Callbacks

    #define handlePtr ((PrivateData*)glfwGetWindowUserPointer(view))

    static void onKeyboardCallback(GLFWwindow* view, int key, int scancode, int action, int mods)
    {
        handlePtr->onPuglKeyboard(action, key, mods);
    }

    static void onMouseCallback(GLFWwindow* view, int button, int action, int mods)
    {
        double x, y;
        glfwGetCursorPos(view, &x, &y);
        handlePtr->onPuglMouse(button, action, (int)x, (int)y, mods);
    }

    static void onScrollCallback(GLFWwindow* view, double dx, double dy)
    {
        double x, y;
        glfwGetCursorPos(view, &x, &y);
        handlePtr->onPuglScroll((int)x, (int)y, dx, dy);
    }

    static void onCloseCallback(GLFWwindow* view)
    {
        handlePtr->onPuglClose();
    }

    static void onDisplayCallback(GLFWwindow* view)
    {
        handlePtr->onPuglDisplay();
    }

    static void onReshapeCallback(GLFWwindow* view, int width, int height)
    {
        handlePtr->onPuglReshape(width, height);
    }

    static void onMotionCallback(GLFWwindow* view, double x, double y)
    {
        handlePtr->onPuglMotion((int)x, (int)y);
    }

#if 0 // XXX
    static int onSpecialCallback(PuglView* view, bool press, PuglKey key)
    {
        return handlePtr->onPuglSpecial(press, static_cast<Key>(key));
    }

    static void fileBrowserSelectedCallback(PuglView* view, const char* filename)
    {
        handlePtr->fSelf->fileBrowserSelected(filename);
    }
#endif // XXX

    #undef handlePtr

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrivateData)
};

// -----------------------------------------------------------------------
// Window

Window::Window(Application& app)
    : pData(new PrivateData(app, this)) {}

Window::Window(Application& app, Window& parent)
    : pData(new PrivateData(app, this, parent)) {}

Window::Window(Application& app, intptr_t parentId)
    : pData(new PrivateData(app, this, parentId)) {}

Window::~Window()
{
    delete pData;
}

void Window::show()
{
    pData->setVisible(true);
}

void Window::hide()
{
    pData->setVisible(false);
}

void Window::close()
{
    pData->close();
}

void Window::exec(bool lockWait)
{
    pData->exec(lockWait);
}

void Window::focus()
{
    pData->focus();
}

void Window::repaint() noexcept
{
    glfwSwapBuffers(pData->fView);
}

// static int fib_filter_filename_filter(const char* const name)
// {
//     return 1;
//     (void)name;
// }

bool Window::openFileBrowser(const FileBrowserOptions& options)
{
#if 0
//#ifdef SOFD_HAVE_X11
    using DISTRHO_NAMESPACE::String;

    // --------------------------------------------------------------------------
    // configure start dir

    // TODO: get abspath if needed
    // TODO: cross-platform

    String startDir(options.startDir);

    if (startDir.isEmpty())
    {
        if (char* const dir_name = get_current_dir_name())
        {
            startDir = dir_name;
            std::free(dir_name);
        }
    }

    DISTRHO_SAFE_ASSERT_RETURN(startDir.isNotEmpty(), false);

    if (! startDir.endsWith('/'))
        startDir += "/";

    DISTRHO_SAFE_ASSERT_RETURN(x_fib_configure(0, startDir) == 0, false);

    // --------------------------------------------------------------------------
    // configure title

    String title(options.title);

    if (title.isEmpty())
    {
        title = pData->getTitle();

        if (title.isEmpty())
            title = "FileBrowser";
    }

    DISTRHO_SAFE_ASSERT_RETURN(x_fib_configure(1, title) == 0, false);

    // --------------------------------------------------------------------------
    // configure filters

    x_fib_cfg_filter_callback(nullptr); //fib_filter_filename_filter);

    // --------------------------------------------------------------------------
    // configure buttons

    x_fib_cfg_buttons(3, options.buttons.listAllFiles-1);
    x_fib_cfg_buttons(1, options.buttons.showHidden-1);
    x_fib_cfg_buttons(2, options.buttons.showPlaces-1);

    // --------------------------------------------------------------------------
    // show

    return (x_fib_show(pData->xDisplay, pData->xWindow, /*options.width*/0, /*options.height*/0) == 0);
#else
    // not implemented
    return false;
#endif
}

bool Window::isVisible() const noexcept
{
    return pData->fVisible;
}

void Window::setVisible(bool yesNo)
{
    pData->setVisible(yesNo);
}

bool Window::isResizable() const noexcept
{
    return pData->fResizable;
}

void Window::setResizable(bool yesNo)
{
    pData->setResizable(yesNo);
}

uint Window::getWidth() const noexcept
{
    return pData->fWidth;
}

uint Window::getHeight() const noexcept
{
    return pData->fHeight;
}

Size<uint> Window::getSize() const noexcept
{
    return Size<uint>(pData->fWidth, pData->fHeight);
}

void Window::setSize(uint width, uint height)
{
    pData->setSize(width, height);
}

void Window::setSize(Size<uint> size)
{
    pData->setSize(size.getWidth(), size.getHeight());
}

const char* Window::getTitle() const noexcept
{
    return pData->getTitle();
}

void Window::setTitle(const char* title)
{
    pData->setTitle(title);
}

void Window::setTransientWinId(uintptr_t winId)
{
    pData->setTransientWinId(winId);
}

Application& Window::getApp() const noexcept
{
    return pData->fApp;
}

intptr_t Window::getWindowId() const noexcept
{
    return glfwGetCurrentContext();
}

void Window::_addWidget(Widget* const widget)
{
    pData->addWidget(widget);
}

void Window::_removeWidget(Widget* const widget)
{
    pData->removeWidget(widget);
}

void Window::_idle()
{
    pData->idle();
}

// -----------------------------------------------------------------------

void Window::addIdleCallback(IdleCallback* const callback)
{
    DISTRHO_SAFE_ASSERT_RETURN(callback != nullptr,)

    pData->fApp.pData->idleCallbacks.push_back(callback);
}

void Window::removeIdleCallback(IdleCallback* const callback)
{
    DISTRHO_SAFE_ASSERT_RETURN(callback != nullptr,)

    pData->fApp.pData->idleCallbacks.remove(callback);
}

// -----------------------------------------------------------------------

void Window::onDisplayBefore()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
}

void Window::onDisplayAfter()
{
    glfwSwapBuffers(pData->fView);
}

void Window::onReshape(uint width, uint height)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(width), static_cast<GLdouble>(height), 0.0, 0.0, 1.0);
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Window::onClose()
{
}

void Window::fileBrowserSelected(const char*)
{
}

// -----------------------------------------------------------------------

StandaloneWindow::StandaloneWindow()
    : Application(),
      Window((Application&)*this),
      fWidget(nullptr) {}

void StandaloneWindow::exec()
{
    Window::show();
    Application::exec();
}

void StandaloneWindow::onReshape(uint width, uint height)
{
    if (fWidget != nullptr)
        fWidget->setSize(width, height);
    Window::onReshape(width, height);
}

void StandaloneWindow::_addWidget(Widget* widget)
{
    if (fWidget == nullptr)
    {
        fWidget = widget;
        fWidget->pData->needsFullViewport = true;
    }
    Window::_addWidget(widget);
}

void StandaloneWindow::_removeWidget(Widget* widget)
{
    if (fWidget == widget)
    {
        fWidget->pData->needsFullViewport = false;
        fWidget = nullptr;
    }
    Window::_removeWidget(widget);
}

// -----------------------------------------------------------------------

END_NAMESPACE_DGL

#undef DBG
#undef DBGF
