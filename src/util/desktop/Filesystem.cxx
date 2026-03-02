#include <Filesystem.hxx>
#include <Settings.hxx>
#include <LOG.hxx>
#include <fstream>
#include "compression.hxx"

#include <SDL.h>

#include <filesystem>

namespace fs
{
using namespace std::filesystem;
} // namespace fs

template <typename Callback> void forEachFileType(fs::path &&path, std::string &&extension, Callback callback)
{
  for (const auto &fp : fs::directory_iterator(path))
  {
    if (static_cast<fs::path>(fp).extension() == extension)
    {
      callback(fp);
    }
  }
}

std::string fs::readCompressedFileAsString(const std::string &fileName)
{
  return decompressString(fs::readFileAsString(fileName, true));
}

std::string fs::readFileAsString(const std::string &fileName, bool binaryMode)
{
  std::ios::openmode mode;
  if (binaryMode)
    mode = std::ios_base::in | std::ios_base::binary;
  else
  {
    mode = std::ios_base::in;
  }

  std::ifstream stream; //(fileName, mode);

  if (fs::fileExists(fileName))
  { // first try given path
    stream.open(fileName, mode);
  }
  else if (fs::fileExists(getBasePath() + fileName))
  { // if this doesn't work, add the basepath
    stream.open(getBasePath() + fileName, mode);
  }
  else
  {
    throw ConfigurationError(TRACE_INFO "File " + fileName + " doesn't exist");
  }

  if (!stream)
  {
    throw ConfigurationError(TRACE_INFO "Can't open file " + fileName);
  }

  std::stringstream buffer;
  buffer << stream.rdbuf();
  stream.close();

  return buffer.str();
}

void fs::writeStringToFileCompressed(const std::string &fileName, const std::string &stringToWrite)
{
  writeStringToFile(fileName, compressString(stringToWrite), true);
}

void fs::writeStringToFile(const std::string &fileName, const std::string &stringToWrite, bool binaryMode)
{
  std::ios::openmode mode;
  if (binaryMode)
  {
    mode = std::ios_base::out | std::ios_base::binary;
  }
  else
  {
    mode = std::ios_base::out;
  }

  std::ofstream stream(fileName, mode);

  if (!stream)
  {
    throw ConfigurationError(TRACE_INFO "Could not write to file " + fileName);
  }

  stream << stringToWrite << std::endl;
  stream.close();
}

std::vector<std::string> fs::getDirectoryListing(const std::string &directory)
{
  std::vector<std::string> dirContent;
  std::string pathToSaveFiles = getBasePath();
  pathToSaveFiles.append(directory);
  for (const auto &it : fs::directory_iterator(fs::path(pathToSaveFiles)))
  {
    dirContent.emplace_back(static_cast<path>(it).string());
  }
  return dirContent;
}

std::vector<std::string> fs::getSaveGamePaths()
{
  std::vector<std::string> saveGames;
  forEachFileType("resources", ".cts",
                  [&saveGames](const auto &path) { saveGames.emplace_back(static_cast<fs::path>(path).string()); });
  return saveGames;
}

bool fs::fileExists(const std::string &filePath) { return fs::exists(fs::path(filePath)); }

std::string fs::getBasePath()
{
  std::string sPath;

  char *path = SDL_GetBasePath();
  if (path)
  {
    sPath = path;
  }
  else
  {
    throw CytopiaError(TRACE_INFO "SDL_GetBasePath() failed!");
  }
  SDL_free(path);

  return sPath;
}

void fs::createDirectory(const std::string &dir)
{
  if (!fs::is_directory(dir))
  {
    if (fs::create_directories(dir.c_str()))
    {
      LOG(LOG_INFO) << "Created directory" << dir;
    }
  }
}
