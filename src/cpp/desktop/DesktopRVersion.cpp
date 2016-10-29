/*
 * DesktopRVersion.cpp
 *
 * Copyright (C) 2009-18 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */
#include "DesktopRVersion.hpp"

#include <windows.h>

#include <QtAlgorithms>
#include <QMessageBox>

#include <core/system/System.hpp>
#include <core/system/Environment.hpp>
#include <core/system/RegistryKey.hpp>

#include "DesktopInfo.hpp"
#include "DesktopChooseRHome.hpp"

#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY 0x0200
#endif
#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY 0x0100
#endif

using namespace rstudio::core;

namespace rstudio {
namespace desktop {

namespace {

template <typename T>
T read(std::ifstream& stream)
{
   T val;
   stream.read(reinterpret_cast<char*>(&val), sizeof(val));
   return val;
}

DWORD getVersion(QString path)
{
   if (!QFile::exists(path))
      return 0;

   DWORD bytesNeeded = ::GetFileVersionInfoSize(path.toLocal8Bit(), nullptr);
   if (bytesNeeded == 0)
      return 0;

   std::vector<BYTE> buffer(bytesNeeded);
   LPVOID pBlock = reinterpret_cast<LPVOID>(&(buffer[0]));

   if (!::GetFileVersionInfo(
         path.toLocal8Bit(),
         0,
         static_cast<DWORD>(buffer.size()),
         pBlock))
   {
      return 0;
   }

   char root[] = "\\";
   VS_FIXEDFILEINFO* pFixedFileInfo;
   UINT len;
   if (!::VerQueryValue(pBlock,
                        root,
                        reinterpret_cast<LPVOID*>(&pFixedFileInfo),
                        &len))
   {
      return 0;
   }

   return pFixedFileInfo->dwFileVersionMS;
}

bool confirmMinVersion(
      DWORD version,
      int major=RSTUDIO_R_MAJOR_VERSION_REQUIRED,
      int minor=RSTUDIO_R_MINOR_VERSION_REQUIRED+RSTUDIO_R_PATCH_VERSION_REQUIRED)
{
   WORD fileMajor = HIWORD(version);
   if (fileMajor > major)
      return true;
   else if (fileMajor < major)
      return false;
   return LOWORD(version) >= minor;
}

Architecture getArch(QString path)
{
   // http://www.microsoft.com/whdc/system/platform/firmware/PECOFF.mspx
   using namespace std;

   if (!QFile::exists(path))
      return ArchNone;

   ifstream stream(path.toLocal8Bit().data(), ifstream::in | ifstream::binary);
   stream.seekg(0x3C, ios::beg);
   uint32_t headerOffset = read<uint32_t>(stream);
   stream.seekg(headerOffset, ios::beg);
   uint32_t header = read<uint32_t>(stream);
   if (header != 0x4550)
      return ArchNone;
   uint16_t arch = read<uint16_t>(stream);

   if (!stream.good())
      return ArchNone;

   switch (arch)
   {
   case IMAGE_FILE_MACHINE_I386:
      return ArchX86;
   case IMAGE_FILE_MACHINE_AMD64:
      return ArchX64;
   default:
      return ArchUnknown;
   }
}

} // anonymous namespace


// Given an R home directory, add candidates for child bin directories
// to the given version list. The versions may not be valid.
void versionsFromRHome(QDir rHome, QList<RVersion>* pResults)
{
   QStringList dirs = QStringList() <<
                      QString::fromUtf8("bin") <<
                      QString::fromUtf8("bin/x64");
   for (int i = 0; i < dirs.size(); i++)
   {
      QDir tmp = rHome;
      if (tmp.cd(dirs.at(i)) && tmp.exists(QString::fromUtf8("R.dll")))
         *pResults << RVersion(tmp.absolutePath());
   }
}

// Given an R bin directory, return our best guess at its home dir.
// It will try even if the bin dir doesn't exist.
QString binDirToHomeDir(QString binDir)
{
   if (binDir.isEmpty())
      return QString();

   QDir dir = binDir;
   if (!dir.isAbsolute())
      return QString();

   // For R-2.12 and later (bin/i386 and bin/x64)
   if (dir.dirName() != QString::fromUtf8("bin"))
      dir.setPath(QDir::cleanPath(dir.filePath(QString::fromUtf8(".."))));

   // The parent of the bin dir is the home dir
   if (dir.dirName() == QString::fromUtf8("bin"))
      return QDir::cleanPath(dir.filePath(QString::fromUtf8("..")));

   return QString();
}

QList<RVersion> detectVersionsInDir(QString dir)
{
   QDir qdir(dir);
   if (qdir.exists(QString::fromUtf8("R.dll")))
      return QList<RVersion>() << RVersion(qdir.absolutePath());

   if (qdir.dirName() == QString::fromUtf8("bin"))
      qdir.setPath(binDirToHomeDir(qdir.absolutePath()));

   QList<RVersion> results;
   versionsFromRHome(qdir, &results);
   return results;
}

// Grovel the given program files dir for R versions. They might not be valid.
void enumProgramFiles(QString progFiles, QList<RVersion>* pResults)
{
   QDir programFiles(progFiles);
   if (programFiles.isAbsolute() && programFiles.exists())
   {
      if (programFiles.cd(QString::fromUtf8("R")))
      {
         QStringList rDirs = programFiles.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
         for (int i = 0; i < rDirs.size(); i++)
         {
            versionsFromRHome(programFiles.absoluteFilePath(rDirs.at(i)),
                              pResults);
         }
      }
   }
}

// Grovel all program files dirs for R versions. They might not be valid.
void enumProgramFiles(QList<RVersion>* pResults)
{
   QStringList progFiles;
   progFiles << QString::fromStdString(system::getenv("ProgramFiles"));
   progFiles << QString::fromStdString(system::getenv("ProgramW6432"));
   progFiles << QString::fromStdString(system::getenv("ProgramFiles(x86)"));
   progFiles.removeAll(QString());
   progFiles.removeDuplicates();

   for (int i = 0; i < progFiles.size(); i++)
      enumProgramFiles(progFiles.at(i), pResults);
}

void enumRegistry(Architecture architecture, HKEY key, QList<RVersion>* pResults)
{
   using namespace rstudio::core::system;

   REGSAM flags;
   switch (architecture)
   {
   case ArchX86:
      flags = KEY_WOW64_32KEY;
      break;
   case ArchX64:
      flags = KEY_WOW64_64KEY;
      break;
   default:
      return;
   }

   RegistryKey regKey;
   Error error = regKey.open(key,
                             "Software\\R-core\\R",
                             KEY_READ | flags);
   if (error)
   {
      if (error.code() != boost::system::errc::no_such_file_or_directory)
         LOG_ERROR(error);
      return;
   }

   std::vector<std::string> keys = regKey.keyNames();
   for (size_t i = 0; i < keys.size(); i++)
   {
      RegistryKey verKey;
      error = verKey.open(regKey.handle(),
                          keys.at(i),
                          KEY_READ | flags);
      if (error)
      {
         LOG_ERROR(error);
         continue;
      }

      std::string installPath = verKey.getStringValue("InstallPath", "");
      if (!installPath.empty())
         versionsFromRHome(QString::fromStdString(installPath), pResults);
   }
}

void enumRegistry(QList<RVersion>* pResults)
{
   enumRegistry(ArchX64, HKEY_CURRENT_USER, pResults);
   enumRegistry(ArchX64, HKEY_LOCAL_MACHINE, pResults);
}

#if defined(CONDA_BUILD)
void enumConda(QList<RVersion>* pResults)
{
   // The CONDA_PREFIX enviornment variable should get preference, followed
   // by a relative path from the executable. This is similar to what we do
   // in REnviornmentPosix.cpp
   char* conda_prefix = getenv("CONDA_PREFIX");
   // New Unix-a-like layout followed by the old Windows layout.
   const char* pfxs[2] = {"/lib/R", "/R"};
   const char* pfxs2[2] = {"/../../../lib/R", "/../../../R"};
   for (int i = 0; i < 2; ++i) {
      if (conda_prefix != NULL)
         versionsFromRHome(QString::fromStdString(std::string(conda_prefix) + pfxs[i]), pResults);
      QDir executable_path = QDir(QCoreApplication::applicationFilePath() + QString::fromLatin1(pfxs2[i])).absolutePath();
      QString wtf = executable_path.absolutePath();
      if (executable_path.exists())
         versionsFromRHome(executable_path, pResults);
   }
}
#endif

// Return all valid versions of R we can find, nicely sorted and de-duped.
// You can explicitly pass in versions that you know about (that may or
// may not be valid) using the versions param.
QList<RVersion> allRVersions(QList<RVersion> versions)
{
   versionsFromRHome(QString::fromStdString(system::getenv("R_HOME")),
                     &versions);
#if defined(CONDA_BUILD)
   enumConda(&versions);
#endif
   enumRegistry(&versions);
   enumProgramFiles(&versions);

   // Remove any invalid versions
   for (int i = 0; i < versions.size(); i++)
   {
      if (!versions.at(i).isValid())
         versions.removeAt(i--);
   }

   // Sort and de-duplicate
   qSort(versions);
   for (int i = 1; i < versions.size(); i++)
   {
      if (versions.at(i) == versions.at(i-1))
         versions.removeAt(i--);
   }

   // Remove unsupported architectures
   QMutableListIterator<RVersion> verList(versions);
   while (verList.hasNext())
   {
      if (verList.next().architecture() != ArchX64)
         verList.remove();
   }

   return versions;
}

RVersion detectPreferredFromRegistry(HKEY key, Architecture architecture)
{
   using namespace rstudio::core::system;

   REGSAM flags;
   switch (architecture)
   {
   case ArchX86:
      flags = KEY_WOW64_32KEY;
      break;
   case ArchX64:
      flags = KEY_WOW64_64KEY;
      break;
   default:
      return RVersion();
   }

   RegistryKey regKey;
   Error error = regKey.open(key,
                             "Software\\R-core\\R",
                             KEY_READ | flags);
   if (error)
   {
      if (error.code() != boost::system::errc::no_such_file_or_directory)
         LOG_ERROR(error);
      return RVersion();
   }

   QList<RVersion> versions;
   versionsFromRHome(
         QString::fromStdString(regKey.getStringValue("InstallPath", "")),
         &versions);
   for (int i = 0; i < versions.size(); i++)
   {
      if (versions.at(i).isValid()
         && versions.at(i).architecture() == architecture)
      {
         return versions.at(i);
      }
   }

   return RVersion();
}

RVersion autoDetect(Architecture architecture, bool preferredOnly)
{
#if !defined(CONDA_BUILD)
   // Disable registry checks for conda, enumRegistry() is checked anyway,
   // and conda doesn't consider the system R to be the preferred version.
   // If anything, enumConda()'s result could be used here instead, though
   // R_HOME still gets preference. RSTUDIO_WHICH_R is not used on Winodws
   RVersion preferred = detectPreferredFromRegistry(HKEY_CURRENT_USER, architecture);
   if (!preferred.isValid())
       preferred = detectPreferredFromRegistry(HKEY_LOCAL_MACHINE, architecture);
   if (preferred.isValid())
      return preferred;
#endif
   if (preferredOnly)
      return RVersion();

   QList<RVersion> versions = allRVersions();
   for (int i = 0; i < versions.size(); i++)
      if (versions.at(i).architecture() == architecture)
         return versions.at(i);

   return RVersion();
}

RVersion autoDetect()
{
   return autoDetect(ArchX64);
}

/*
Looks for a valid R directory in the following places:
- Value of %R_HOME%
- Value of HKEY_LOCAL_MACHINE\Software\R-core\R@InstallPath
    (64-bit keys)
- Values under HKEY_LOCAL_MACHINE\Software\R-core\R\*@InstallPath
    (64-bit keys)
- Enumerate %ProgramFiles% directory (64-bit dirs)

If forceUi is true, we always show the picker dialog.
Otherwise, we try to do our best to match the user's specified wishes,
and if we can't succeed then we show the picker dialog.
*/
RVersion detectRVersion(bool forceUi, QWidget* parent)
{
   Options& options = desktop::options();

   RVersion rVersion;

   // if currently select R version is 32-bit, ignore it
   RVersion rCurrentVersion(options.rBinDir());
   if (!rCurrentVersion.isEmpty() && rCurrentVersion.architecture() == ArchX64)
   {
      rVersion = rCurrentVersion;
   }

   if (!forceUi)
   {
      if (!rVersion.isEmpty())
      {
         // User manually chose an R version. Use it if it's valid.
         if (rVersion.isValid())
            return rVersion;
      }
      else
      {
         // User wants us to autodetect (or didn't specify--autodetect
         // is the default).
         RVersion autoDetected = autoDetect();
         if (autoDetected.isValid())
            return autoDetected;
      }
   }

   // Either forceUi was true, xor the manually specified R version is
   // no longer valid, xor we tried to autodetect and failed.
   // Now we show the dialog and make the user choose.
   QString renderingEngine = desktop::options().desktopRenderingEngine();
   ChooseRHome dialog(allRVersions(QList<RVersion>() << rVersion), parent);
   dialog.setVersion(rVersion);
   dialog.setRenderingEngine(renderingEngine);
   if (dialog.exec() == QDialog::Accepted)
   {
      // Keep in mind this value might be "", if the user indicated
      // they want to use the system default. The dialog won't let
      // itself be accepted unless a valid installation is detected.
      rVersion = dialog.version();
      options.setRBinDir(rVersion.binDir());
      options.setDesktopRenderingEngine(dialog.renderingEngine());

      // If we changed the rendering engine, we'll have to restart
      // RStudio. Show the user a message and request that they
      // restart the application.
      if (renderingEngine != dialog.renderingEngine())
      {
         QMessageBox* messageBox = new QMessageBox(nullptr);
         messageBox->setAttribute(Qt::WA_DeleteOnClose, true);
         messageBox->setIcon(QMessageBox::Information);
         messageBox->setWindowIcon(QIcon(QStringLiteral(":/icons/RStudio.ico")));
         messageBox->setWindowTitle(QStringLiteral("Rendering Engine Changed"));
         messageBox->setText(QStringLiteral(
                  "The desktop rendering engine has been changed. "
                  "Please restart RStudio for these changes to take effect."));
         messageBox->exec();
         
         return QString();
      }
      
      // Recurse. The ChooseRHome dialog should've validated that
      // the values are acceptable, so this recursion will never
      // go more than one level deep (i.e. this call should never
      // result in the dialog being shown again).
      return detectRVersion(false, parent);
   }

   // We couldn't autodetect a string and the user bailed on the
   // dialog. No R_HOME is available.
   return QString();
}

RVersion::RVersion(QString binDir) :
      binDir_(binDir),
      homeDir_(binDirToHomeDir(binDir)),
      version_(0),
      arch_(ArchNone)
{
   if (!binDir.isEmpty())
   {
      QString dllPath = QDir(binDir_).absoluteFilePath(QString::fromUtf8("R.dll"));
      version_ = getVersion(dllPath);
      arch_ = getArch(dllPath);
   }
}

QString RVersion::binDir() const
{
   return binDir_;
}

QString RVersion::homeDir() const
{
   return homeDir_;
}

QString RVersion::description() const
{
   QString result;

   if (architecture() == ArchX64)
      result.append(QString::fromUtf8("[64-bit] "));
   else if (architecture() == ArchX86)
      result.append(QString::fromUtf8("[32-bit] "));

   result.append(QDir::toNativeSeparators(homeDir_));

   return result;
}

bool RVersion::isEmpty() const
{
   return binDir_.isEmpty();
}

bool RVersion::isValid() const
{
   return validate() == ValidateSuccess;
}

ValidateResult RVersion::validate() const
{
   if (isEmpty() || homeDir_.isEmpty())
      return ValidateNotFound;

   QDir binDir(binDir_);
   if (!binDir.exists(QString::fromUtf8("R.dll")))
      return ValidateNotFound;

   if (!confirmMinVersion(version()))
      return ValidateVersionTooOld;

   return ValidateSuccess;
}

quint32 RVersion::version() const
{
   return version_;
}

Architecture RVersion::architecture() const
{
   return arch_;
}

int RVersion::compareTo(const RVersion& other) const
{
   int c;

   // First order by version, descending
   c = -(static_cast<int>(version()) - static_cast<int>(other.version()));
   if (c != 0)
      return c;

   // Then by homedir
   c = QString::compare(homeDir(), other.homeDir(), Qt::CaseInsensitive);
   if (c != 0)
      return c;

   // Then put 64-bit first
   c = -(architecture() - other.architecture());
   if (c != 0)
      return c;

   // Then order by bindir
   c = QString::compare(binDir(), other.binDir(), Qt::CaseInsensitive);
   if (c != 0)
      return c;

   return 0;
}

bool RVersion::operator<(const RVersion& other) const
{
   return compareTo(other) < 0;
}

bool RVersion::operator==(const RVersion& other) const
{
   return QString::compare(binDir_, other.binDir_, Qt::CaseInsensitive) == 0;
}


} // namespace desktop
} // namespace rstudio
