#include "OSystem.hxx"
#include "LOG.hxx"
#include "platform.hxx"

#include <ctime>

#if defined(CYTOPIA_PLATFORM_LINUX) || defined(CYTOPIA_PLATFORM_HAIKU) || defined(CYTOPIA_PLATFORM_MACOSX)
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <vector>

static int executeNoShell(const std::vector<std::string> &args, bool background = false)
{
  if (args.empty())
    return -1;

  pid_t pid = fork();
  if (pid == 0)
  {
    if (background)
    {
      pid_t pid2 = fork();
      if (pid2 > 0)
      {
        _exit(0);
      }
      else if (pid2 < 0)
      {
        _exit(1);
      }
    }

    std::vector<char *> c_args;
    for (const auto &arg : args)
    {
      c_args.push_back(const_cast<char *>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(c_args[0], c_args.data());
    _exit(1);
  }
  else if (pid > 0)
  {
    int status = 0;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
  }
  return -1;
}
#endif

#ifdef CYTOPIA_PLATFORM_LINUX
#include <cstdlib>
#include <string.h>

const char *getDialogCommand()
{
  if (executeNoShell({"which", "gdialog"}) == 0)
    return "gdialog";

  else if (executeNoShell({"which", "kdialog"}) == 0)
    return "kdialog";

  return nullptr;
}
#elif defined(CYTOPIA_PLATFORM_MACOSX)
#include <cstdlib>
#elif defined(CYTOPIA_PLATFORM_WIN)
#include <windows.h>
#endif

void OSystem::error(const std::string &title, const std::string &text)
{
#if defined(CYTOPIA_PLATFORM_LINUX)
  const char *dialogCommand = getDialogCommand();
  if (dialogCommand)
  {
    if (executeNoShell({dialogCommand, "--title", title, "--msgbox", text}) != 0)
    {
      LOG(LOG_DEBUG) << "WARNING: Cant execute command " << dialogCommand;
    }
  }

  // fail-safe method here, using stdio perhaps, depends on your application
#elif defined(CYTOPIA_PLATFORM_WIN)
  MessageBox(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
#endif
}

void OSystem::openUrl(const std::string &url, const std::string &prefix)
{
#ifdef CYTOPIA_PLATFORM_LINUX
  (void)prefix;
  executeNoShell({"xdg-open", url});

#elif defined(CYTOPIA_PLATFORM_WIN)
  (void)prefix;
  ShellExecuteA(0, "Open", url.c_str(), 0, 0, SW_SHOW);

#elif defined(CYTOPIA_PLATFORM_MACOSX)
  (void)prefix;
  executeNoShell({"open", url}, true);
#endif
}

void OSystem::openDir(const std::string &path, const std::string &prefix)
{
#ifdef CYTOPIA_PLATFORM_LINUX
  (void)prefix;
  executeNoShell({"nautilus", path}, true);
#elif defined(CYTOPIA_PLATFORM_WIN)
  (void)prefix;
  ShellExecute(GetDesktopWindow(), "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

#elif defined(CYTOPIA_PLATFORM_MACOSX)
  (void)prefix;
  executeNoShell({"open", path}, true);

#endif
}

bool OSystem::is(OSystem::Type type)
{
#define RETURN_TRUE(t)                                                                                                           \
  if (type == t)                                                                                                                 \
    return true;

#ifdef CYTOPIA_PLATFORM_WIN
  RETURN_TRUE(Type::windows)
#endif

#ifdef CYTOPIA_PLATFORM_LINUX
  RETURN_TRUE(Type::linux)
#endif

#ifdef CYTOPIA_PLATFORM_UNIX
  RETURN_TRUE(Type::unix)
#endif

#ifdef CYTOPIA_PLATFORM_ANDROID
  RETURN_TRUE(Type::android)
#endif

#ifdef CYTOPIA_PLATFORM_MACOSX
  RETURN_TRUE(Type::macos)
#endif

#ifdef CYTOPIA_PLATFORM_XBSD
  RETURN_TRUE(Type::bsd)
#endif

#ifdef CYTOPIA_PLATFORM_HAIKU
  RETURN_TRUE(Type::haiku)
#endif

#ifdef CYTOPIA_PLATFORM_BEOS
  RETURN_TRUE(Type::beos)
#endif

  return false;
}

bool OSystem::isAndroid() { return is(Type::android); }
bool OSystem::isLinux() { return is(Type::linux); }
bool OSystem::isUnix() { return is(Type::unix); }
bool OSystem::isMac() { return is(Type::macos); }
bool OSystem::isWindows() { return is(Type::windows); }