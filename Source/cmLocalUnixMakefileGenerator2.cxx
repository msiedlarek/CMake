/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmLocalUnixMakefileGenerator2.h"

#include "cmGeneratedFileStream.h"
#include "cmGlobalGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"

#include <queue>

//----------------------------------------------------------------------------
cmLocalUnixMakefileGenerator2::cmLocalUnixMakefileGenerator2()
{
}

//----------------------------------------------------------------------------
cmLocalUnixMakefileGenerator2::~cmLocalUnixMakefileGenerator2()
{
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::Generate(bool fromTheTop)
{
  // TODO: Account for control-c during Makefile generation.

  // Generate old style for now.
  this->cmLocalUnixMakefileGenerator::Generate(fromTheTop);

  // Generate the rule files for each target.
  const cmTargets& targets = m_Makefile->GetTargets();
  for(cmTargets::const_iterator t = targets.begin(); t != targets.end(); ++t)
    {
    // TODO: Dispatch generation of each target type.
    if((t->second.GetType() == cmTarget::EXECUTABLE) ||
       (t->second.GetType() == cmTarget::STATIC_LIBRARY) ||
       (t->second.GetType() == cmTarget::SHARED_LIBRARY) ||
       (t->second.GetType() == cmTarget::MODULE_LIBRARY))
      {
      this->GenerateTargetRuleFile(t->second);
      }
    }

  // Generate the main makefile.
  this->GenerateMakefile();

  // Generate the cmake file that keeps the makefile up to date.
  this->GenerateCMakefile();
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::GenerateMakefile()
{
  // Open the output file.
  std::string makefileName = m_Makefile->GetStartOutputDirectory();
  makefileName += "/Makefile2";
  std::ofstream makefileStream(makefileName.c_str());
  if(!makefileStream)
    {
    cmSystemTools::Error("Error can not open for write: ",
                         makefileName.c_str());
    cmSystemTools::ReportLastSystemError("");
    return;
    }

  // Write the do not edit header.
  this->WriteDisclaimer(makefileStream);

  // Write standard variables to the makefile.
  this->WriteMakeVariables(makefileStream);

  // Write special targets that belong at the top of the file.
  this->WriteSpecialTargetsTop(makefileStream);

  // Write the directory-level build rules.
  this->WriteAllRule(makefileStream);

  // Write include statements to get rules for each target.
  this->WriteTargetIncludes(makefileStream);

  // Write jump-and-build rules that were recorded in the map.
  this->WriteJumpAndBuildRules(makefileStream);

  // Write special targets that belong at the bottom of the file.
  this->WriteSpecialTargetsBottom(makefileStream);
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::GenerateCMakefile()
{
  std::string makefileName = m_Makefile->GetStartOutputDirectory();
  makefileName += "/Makefile2";
  std::string cmakefileName = makefileName;
  cmakefileName += ".cmake";

  // Open the output file.
  std::ofstream cmakefileStream(cmakefileName.c_str());
  if(!cmakefileStream)
    {
    cmSystemTools::Error("Error can not open for write: ",
                         cmakefileName.c_str());
    cmSystemTools::ReportLastSystemError("");
    return;
    }

  // Write the do not edit header.
  this->WriteDisclaimer(cmakefileStream);

  // Get the list of files contributing to this generation step.
  // Sort the list and remove duplicates.
  std::vector<std::string> lfiles = m_Makefile->GetListFiles();
  std::sort(lfiles.begin(), lfiles.end(), std::less<std::string>());
  std::vector<std::string>::iterator new_end = std::unique(lfiles.begin(),
                                                           lfiles.end());
  lfiles.erase(new_end, lfiles.end());

  // Save the list to the cmake file.
  cmakefileStream
    << "# The corresponding makefile\n"
    << "# \"" << makefileName << "\"\n"
    << "# was generated from the following files:\n"
    << "SET(CMAKE_MAKEFILE_DEPENDS\n"
    << "  \"" << m_Makefile->GetHomeOutputDirectory() << "/CMakeCache.txt\"\n";
  for(std::vector<std::string>::const_iterator i = lfiles.begin();
      i !=  lfiles.end(); ++i)
    {
    cmakefileStream
      << "  \"" << i->c_str() << "\"\n";
    }
  cmakefileStream
    << "  )\n\n";

  // Set the corresponding makefile in the cmake file.
  cmakefileStream
    << "# The corresponding makefile is:\n"
    << "SET(CMAKE_MAKEFILE_OUTPUTS\n"
    << "  \"" << makefileName.c_str() << "\"\n"
    << "  )\n\n";

  // Set the set of files to check for dependency integrity.
  cmakefileStream
    << "# The set of files whose dependency integrity should be checked:\n"
    << "SET(CMAKE_DEPENDS_CHECK\n";
  for(std::set<cmStdString>::const_iterator i = m_CheckDependFiles.begin();
      i != m_CheckDependFiles.end(); ++i)
    {
    cmakefileStream
      << "  \"" << i->c_str() << "\"\n";
    }
  cmakefileStream
    << "  )\n";
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::GenerateTargetRuleFile(const cmTarget& target)
{
  // Create a directory for this target.
  std::string dir = this->GetTargetDirectory(target);
  cmSystemTools::MakeDirectory(this->ConvertToFullPath(dir).c_str());

  // First generate the object rule files.  Save a list of all object
  // files for this target.
  std::vector<std::string> objects;
  const std::vector<cmSourceFile*>& sources = target.GetSourceFiles();
  for(std::vector<cmSourceFile*>::const_iterator source = sources.begin();
      source != sources.end(); ++source)
    {
    if(!(*source)->GetPropertyAsBool("HEADER_FILE_ONLY") &&
       !(*source)->GetCustomCommand() &&
       !m_GlobalGenerator->IgnoreFile((*source)->GetSourceExtension().c_str()))
      {
      // Generate this object file's rule file.
      this->GenerateObjectRuleFile(target, *(*source));

      // Save the object file name.
      objects.push_back(this->GetObjectFileName(target, *(*source)));
      }
    }

  // Generate the build-time dependencies file for this target.
  std::string depBase = dir;
  depBase += "/";
  depBase += target.GetName();
  std::string depMakeFile = this->GenerateDependsMakeFile(depBase.c_str());

  // Open the rule file.  This should be copy-if-different because the
  // rules may depend on this file itself.
  std::string ruleFileName = dir;
  ruleFileName += "/";
  ruleFileName += target.GetName();
  ruleFileName += ".make";
  std::string ruleFileNameFull = this->ConvertToFullPath(ruleFileName);
  cmGeneratedFileStream ruleFile(ruleFileNameFull.c_str());
  std::ostream& ruleFileStream = ruleFile.GetStream();
  if(!ruleFileStream)
    {
    // TODO: Produce error message that accounts for generated stream
    // .tmp.
    return;
    }
  this->WriteDisclaimer(ruleFileStream);
  ruleFileStream
    << "# Rule file for target " << target.GetName() << ".\n\n";

  // Include the dependencies for the target.
  ruleFileStream
    << "# Include any dependencies generated for this rule.\n"
    << m_IncludeDirective << " "
    << this->ConvertToOutputForExisting(depMakeFile.c_str()).c_str()
    << "\n\n";

  // Include the rule file for each object.
  if(!objects.empty())
    {
    ruleFileStream
      << "# Include make rules for object files.\n";
    for(std::vector<std::string>::const_iterator obj = objects.begin();
        obj != objects.end(); ++obj)
      {
      std::string objRuleFileName = *obj;
      objRuleFileName += ".make";
      ruleFileStream
        << m_IncludeDirective << " "
        << this->ConvertToOutputForExisting(objRuleFileName.c_str()).c_str()
        << "\n";
      }
    ruleFileStream
      << "\n";
    }

  // Write the dependency generation rule.
  {
  std::vector<std::string> depends;
  std::vector<std::string> no_commands;
  std::string depEcho = "Building dependencies for ";
  depEcho += target.GetName();
  depEcho += "...";
  std::string depTarget = dir;
  depTarget += "/";
  depTarget += target.GetName();
  depTarget += ".depends";
  for(std::vector<std::string>::const_iterator obj = objects.begin();
      obj != objects.end(); ++obj)
    {
    depends.push_back((*obj)+".depends");
    }
  depends.push_back(ruleFileName);
  this->WriteMakeRule(ruleFileStream, 0, depEcho.c_str(),
                      depTarget.c_str(), depends, no_commands);
  }

  // Write the build rule.
  switch(target.GetType())
    {
    case cmTarget::STATIC_LIBRARY:
      this->WriteStaticLibraryRule(ruleFileStream, ruleFileName.c_str(),
                                   target, objects);
      break;
    case cmTarget::SHARED_LIBRARY:
      this->WriteSharedLibraryRule(ruleFileStream, ruleFileName.c_str(),
                                   target, objects);
      break;
    case cmTarget::MODULE_LIBRARY:
      this->WriteModuleLibraryRule(ruleFileStream, ruleFileName.c_str(),
                                   target, objects);
      break;
    case cmTarget::EXECUTABLE:
      this->WriteExecutableRule(ruleFileStream, ruleFileName.c_str(),
                                target, objects);
      break;
    default:
      break;
    }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::GenerateObjectRuleFile(const cmTarget& target, const cmSourceFile& source)
{
  // Identify the language of the source file.
  const char* lang = this->GetSourceFileLanguage(source);
  if(!lang)
    {
    // If language is not known, this is an error.
    cmSystemTools::Error("Source file \"", source.GetFullPath().c_str(),
                         "\" has unknown type.");
    return;
    }

  // Get the full path name of the object file.
  std::string obj = this->GetObjectFileName(target, source);

  // The object file should be checked for dependency integrity.
  m_CheckDependFiles.insert(obj);

  // Create the directory containing the object file.  This may be a
  // subdirectory under the target's directory.
  std::string dir = cmSystemTools::GetFilenamePath(obj.c_str());
  cmSystemTools::MakeDirectory(this->ConvertToFullPath(dir).c_str());

  // Generate the build-time dependencies file for this object file.
  std::string depMakeFile = this->GenerateDependsMakeFile(obj.c_str());

  // Open the rule file for writing.  This should be copy-if-different
  // because the rules may depend on this file itself.
  std::string ruleFileName = obj;
  ruleFileName += ".make";
  std::string ruleFileNameFull = this->ConvertToFullPath(ruleFileName);
  cmGeneratedFileStream ruleFile(ruleFileNameFull.c_str());
  std::ostream& ruleFileStream = ruleFile.GetStream();
  if(!ruleFileStream)
    {
    // TODO: Produce error message that accounts for generated stream
    // .tmp.
    return;
    }
  this->WriteDisclaimer(ruleFileStream);
  ruleFileStream
    << "# Rule file for object file " << obj.c_str() << ".\n\n";

  // Include the dependencies for the target.
  ruleFileStream
    << "# Include any dependencies generated for this rule.\n"
    << m_IncludeDirective << " "
    << this->ConvertToOutputForExisting(depMakeFile.c_str()).c_str()
    << "\n\n";

  // Create the list of dependencies known at cmake time.  These are
  // shared between the object file and dependency scanning rule.
  std::vector<std::string> depends;
  depends.push_back(source.GetFullPath());
  if(const char* objectDeps = source.GetProperty("OBJECT_DEPENDS"))
    {
    std::vector<std::string> deps;
    cmSystemTools::ExpandListArgument(objectDeps, deps);
    for(std::vector<std::string>::iterator i = deps.begin();
        i != deps.end(); ++i)
      {
      depends.push_back(this->ConvertToRelativeOutputPath(i->c_str()));
      }
    }
  depends.push_back(ruleFileName);

  // Write the dependency generation rule.
  std::string depTarget = obj;
  depTarget += ".depends";
  {
  std::string depEcho = "Scanning ";
  depEcho += lang;
  depEcho += " dependencies of ";
  depEcho += this->ConvertToRelativeOutputPath(obj.c_str());
  depEcho += "...";

  // Add a command to call CMake to scan dependencies.  CMake will
  // touch the corresponding depends file after scanning dependencies.
  cmOStringStream depCmd;
  // TODO: Account for source file properties and directory-level
  // definitions when scanning for dependencies.
  depCmd << "$(CMAKE_COMMAND) -E cmake_depends " << lang << " "
         << this->ConvertToRelativeOutputPath(obj.c_str()) << " "
         << this->ConvertToRelativeOutputPath(source.GetFullPath().c_str());
  std::vector<std::string> includeDirs;
  this->GetIncludeDirectories(includeDirs);
  for(std::vector<std::string>::iterator i = includeDirs.begin();
      i != includeDirs.end(); ++i)
    {
    depCmd << " -I" << this->ConvertToRelativeOutputPath(i->c_str());
    }
  std::vector<std::string> commands;
  commands.push_back(depCmd.str());

  // Write the rule.
  this->WriteMakeRule(ruleFileStream, 0, depEcho.c_str(),
                      depTarget.c_str(), depends, commands);
  }

  // Write the build rule.
  {
  // Build the set of compiler flags.
  std::string flags;

  // Add the export symbol definition for shared library objects.
  bool shared = ((target.GetType() == cmTarget::SHARED_LIBRARY) ||
                 (target.GetType() == cmTarget::MODULE_LIBRARY));
  if(shared)
    {
    flags += "-D";
    if(const char* custom_export_name = target.GetProperty("DEFINE_SYMBOL"))
      {
      flags += custom_export_name;
      }
    else
      {
      std::string in = target.GetName();
      in += "_EXPORTS";
      flags += cmSystemTools::MakeCindentifier(in.c_str());
      }
    }

  // Add flags from source file properties.
  this->AppendFlags(flags, source.GetProperty("COMPILE_FLAGS"));

  // Add language-specific flags.
  this->AddLanguageFlags(flags, lang);

  // Add shared-library flags if needed.
  this->AddSharedFlags(flags, lang, shared);

  // Add include directory flags.
  this->AppendFlags(flags, this->GetIncludeFlags(lang));

  // Get the output paths for source and object files.
  std::string sourceFile =
    this->ConvertToRelativeOutputPath(source.GetFullPath().c_str());
  std::string objectFile =
    this->ConvertToRelativeOutputPath(obj.c_str());

  // Construct the compile rules.
  std::vector<std::string> commands;
  std::string compileRuleVar = "CMAKE_";
  compileRuleVar += lang;
  compileRuleVar += "_COMPILE_OBJECT";
  std::string compileRule =
    m_Makefile->GetRequiredDefinition(compileRuleVar.c_str());
  cmSystemTools::ExpandListArgument(compileRule, commands);

  // Expand placeholders in the commands.
  for(std::vector<std::string>::iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    this->ExpandRuleVariables(*i,
                              lang,
                              0, // no objects
                              0, // no target
                              0, // no link libs
                              sourceFile.c_str(),
                              objectFile.c_str(),
                              flags.c_str());
    }

  // Write the rule.
  std::string buildEcho = "Building ";
  buildEcho += lang;
  buildEcho += " object ";
  buildEcho += this->ConvertToRelativeOutputPath(obj.c_str());
  buildEcho += "...";
  this->WriteMakeRule(ruleFileStream, 0, buildEcho.c_str(),
                      obj.c_str(), depends, commands);
  }
}

//----------------------------------------------------------------------------
std::string
cmLocalUnixMakefileGenerator2
::GenerateDependsMakeFile(const char* file)
{
  // Check if the build-time dependencies file exists.
  std::string depMarkFile = file;
  depMarkFile += ".depends";
  std::string depMakeFile = depMarkFile;
  depMakeFile += ".make";
  std::string depMakeFileFull = this->ConvertToFullPath(depMakeFile);
  if(cmSystemTools::FileExists(depMakeFileFull.c_str()))
    {
    // The build-time dependencies file already exists.  Check it.
    this->CheckDependencies(m_Makefile->GetStartOutputDirectory(), file);
    }
  else
    {
    // The build-time dependencies file does not exist.  Create an
    // empty one.
    std::string depMarkFileFull = this->ConvertToFullPath(depMarkFile);
    this->WriteEmptyDependMakeFile(file, depMarkFileFull.c_str(),
                                   depMakeFileFull.c_str());
    }
  return depMakeFile;
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteMakeRule(std::ostream& os,
                const char* comment,
                const char* preEcho,
                const char* target,
                const std::vector<std::string>& depends,
                const std::vector<std::string>& commands,
                const char* postEcho)
{
  // Make sure there is a target.
  if(!target || !*target)
    {
    cmSystemTools::Error("No target for WriteMakeRule!");
    return;
    }

  std::string replace;

  // Write the comment describing the rule in the makefile.
  if(comment)
    {
    replace = comment;
    m_Makefile->ExpandVariablesInString(replace);
    std::string::size_type lpos = 0;
    std::string::size_type rpos;
    while((rpos = replace.find('\n', lpos)) != std::string::npos)
      {
      os << "# " << replace.substr(lpos, rpos-lpos);
      lpos = rpos+1;
      }
    os << "# " << replace.substr(lpos) << "\n";
    }

  // Construct the left hand side of the rule.
  replace = target;
  m_Makefile->ExpandVariablesInString(replace);
  std::string tgt = this->ConvertToRelativeOutputPath(replace.c_str());
  tgt = this->ConvertToMakeTarget(tgt.c_str());
  const char* space = "";
  if(tgt.size() == 1)
    {
    // Add a space before the ":" to avoid drive letter confusion on
    // Windows.
    space = " ";
    }

  // Write the rule.
  if(depends.empty())
    {
    // No dependencies.  The commands will always run.
    os << tgt.c_str() << space << ":\n";
    }
  else
    {
    // Split dependencies into multiple rule lines.  This allows for
    // very long dependency lists even on older make implementations.
    for(std::vector<std::string>::const_iterator dep = depends.begin();
        dep != depends.end(); ++dep)
      {
      replace = *dep;
      m_Makefile->ExpandVariablesInString(replace);
      replace = this->ConvertToRelativeOutputPath(replace.c_str());
      replace = this->ConvertToMakeTarget(replace.c_str());
      os << tgt.c_str() << space << ": " << replace.c_str() << "\n";
      }
    }

  // Write the list of commands.
  bool first = true;
  for(std::vector<std::string>::const_iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    replace = *i;
    m_Makefile->ExpandVariablesInString(replace);
    if(first && preEcho)
      {
      this->OutputEcho(os, preEcho);
      }
    os << "\t" << replace.c_str() << "\n";
    first = false;
    }
  if(postEcho)
    {
    this->OutputEcho(os, postEcho);
    }
  os << "\n";
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::WriteDivider(std::ostream& os)
{
  os
    << "#======================================"
    << "=======================================\n";
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::WriteDisclaimer(std::ostream& os)
{
  os
    << "# CMAKE generated file: DO NOT EDIT!\n"
    << "# Generated by \"" << m_GlobalGenerator->GetName() << "\""
    << " Generator, CMake Version "
    << cmMakefile::GetMajorVersion() << "."
    << cmMakefile::GetMinorVersion() << "\n\n";
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteMakeVariables(std::ostream& makefileStream)
{
  this->WriteDivider(makefileStream);
  makefileStream
    << "# Set environment variables for the build.\n"
    << "\n";
  if(m_WindowsShell)
    {
    makefileStream
      << "!IF \"$(OS)\" == \"Windows_NT\"\n"
      << "NULL=\n"
      << "!ELSE\n"
      << "NULL=nul\n"
      << "!ENDIF\n";
    }
  else
    {
    makefileStream
      << "# The shell in which to execute make rules.\n"
      << "SHELL = /bin/sh\n"
      << "\n";
    }

  std::string cmakecommand =
    this->ConvertToOutputForExisting(
      m_Makefile->GetRequiredDefinition("CMAKE_COMMAND"));
  makefileStream
    << "# The CMake executable.\n"
    << "CMAKE_COMMAND = " << cmakecommand.c_str() << "\n"
    << "\n";
  makefileStream
    << "# The command to remove a file.\n"
    << "RM = " << cmakecommand.c_str() << " -E remove -f\n"
    << "\n";

  if(m_Makefile->GetDefinition("CMAKE_EDIT_COMMAND"))
    {
    makefileStream
      << "# The program to use to edit the cache.\n"
      << "CMAKE_EDIT_COMMAND = "
      << (this->ConvertToOutputForExisting(
            m_Makefile->GetDefinition("CMAKE_EDIT_COMMAND"))) << "\n"
      << "\n";
    }

  makefileStream
    << "# The source directory corresponding to this makefile.\n"
    << "CMAKE_CURRENT_SOURCE = "
    << this->ConvertToRelativeOutputPath(m_Makefile->GetStartDirectory())
    << "\n"
    << "\n";
  makefileStream
    << "# The build directory corresponding to this makefile.\n"
    << "CMAKE_CURRENT_BINARY = "
    << this->ConvertToRelativeOutputPath(m_Makefile->GetStartOutputDirectory())
    << "\n"
    << "\n";
  makefileStream
    << "# The top-level source directory on which CMake was run.\n"
    << "CMAKE_SOURCE_DIR = "
    << this->ConvertToRelativeOutputPath(m_Makefile->GetHomeDirectory())
    << "\n"
    << "\n";
  makefileStream
    << "# The top-level build directory on which CMake was run.\n"
    << "CMAKE_BINARY_DIR = "
    << this->ConvertToRelativeOutputPath(m_Makefile->GetHomeOutputDirectory())
    << "\n"
    << "\n";
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteSpecialTargetsTop(std::ostream& makefileStream)
{
  this->WriteDivider(makefileStream);
  makefileStream
    << "# Special targets provided by cmake.\n"
    << "\n";

  // Build command to run CMake to check if anything needs regenerating.
  std::string cmakefileName = m_Makefile->GetStartOutputDirectory();
  cmakefileName += "/Makefile2.cmake";
  std::string runRule =
    "@$(CMAKE_COMMAND) -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)";
  runRule += " --check-build-system ";
  runRule += this->ConvertToRelativeOutputPath(cmakefileName.c_str());

  // Write the main entry point target.  This must be the VERY first
  // target so that make with no arguments will run it.
  {
  std::vector<std::string> depends;
  std::vector<std::string> commands;
  // Check the build system in this directory.
  depends.push_back("cmake_check_build_system");

  // Recursively build dependencies.
  commands.push_back(this->GetRecursiveMakeCall("all.depends"));

  // Recursively build targets.
  commands.push_back(this->GetRecursiveMakeCall("all.build"));

  // Write the rule.
  std::string preEcho = "Entering directory ";
  preEcho += m_Makefile->GetStartOutputDirectory();
  std::string postEcho = "Finished directory ";
  postEcho += m_Makefile->GetStartOutputDirectory();
  this->WriteMakeRule(makefileStream,
                      "Default target executed when no arguments are "
                      "given to make.",
                      preEcho.c_str(),
                      "all",
                      depends,
                      commands,
                      postEcho.c_str());
  }

  // Write special "cmake_check_build_system" target to run cmake with
  // the --check-build-system flag.
  {
  std::vector<std::string> no_depends;
  std::vector<std::string> commands;
  commands.push_back(runRule);
  this->WriteMakeRule(makefileStream,
                      "Special rule to run CMake to check the build system "
                      "integrity.",
                      "Checking build system integrity...",
                      "cmake_check_build_system",
                      no_depends,
                      commands);
  }

  // Write special "rebuild_cache" target to re-run cmake.
  {
  std::vector<std::string> no_depends;
  std::vector<std::string> commands;
  commands.push_back(
    "$(CMAKE_COMMAND) -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)");
  this->WriteMakeRule(makefileStream,
                      "Special rule to re-run CMake using make.",
                      "Running CMake to regenerate build system...",
                      "rebuild_cache",
                      no_depends,
                      commands);
  }

  // Use CMAKE_EDIT_COMMAND for the edit_cache rule if it is defined.
  // Otherwise default to the interactive command-line interface.
  if(m_Makefile->GetDefinition("CMAKE_EDIT_COMMAND"))
    {
    std::vector<std::string> no_depends;
    std::vector<std::string> commands;
    commands.push_back(
      "$(CMAKE_EDIT_COMMAND) -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)");
    this->WriteMakeRule(makefileStream,
                        "Special rule to re-run CMake cache editor using make.",
                        "Running CMake cache editor...",
                        "edit_cache",
                        no_depends,
                        commands);
    }
  else
    {
    std::vector<std::string> no_depends;
    std::vector<std::string> commands;
    commands.push_back(
      "$(CMAKE_COMMAND) -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) -i");
    this->WriteMakeRule(makefileStream,
                        "Special rule to re-run CMake cache editor using make.",
                        "Running interactive CMake command-line interface...",
                        "edit_cache",
                        no_depends,
                        commands);
    }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteSpecialTargetsBottom(std::ostream& makefileStream)
{
  this->WriteDivider(makefileStream);
  makefileStream
    << "# Special targets to cleanup operation of make.\n"
    << "\n";

  std::vector<std::string> no_commands;

  // Write special target to silence make output.  This must be after
  // the default target in case VERBOSE is set (which changes the name).
  if(!m_Makefile->IsOn("CMAKE_VERBOSE_MAKEFILE"))
    {
    std::vector<std::string> no_depends;
    this->WriteMakeRule(makefileStream,
                        "Suppress display of executed commands.",
                        0,
                        "$(VERBOSE).SILENT",
                        no_depends,
                        no_commands);
    }

  // Special target to cleanup operation of make tool.
  std::vector<std::string> depends;
  depends.push_back(".hpux_make_must_have_this_dependency_here");
  this->WriteMakeRule(makefileStream,
                      "Disable some common implicit rules to speed things up.",
                      0,
                      ".SUFFIXES",
                      depends,
                      no_commands);
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteAllRule(std::ostream& makefileStream)
{
  // Write section header.
  this->WriteDivider(makefileStream);
  makefileStream
    << "# Main rules for this directory.\n"
    << "\n";

  const cmTargets& targets = m_Makefile->GetTargets();

  // Output top level dependency rule.
  {
  std::vector<std::string> depends;
  std::vector<std::string> commands;
  for(cmTargets::const_iterator t = targets.begin(); t != targets.end(); ++t)
    {
    // TODO: Dispatch generation of each target type.
    if((t->second.GetType() == cmTarget::EXECUTABLE) ||
       (t->second.GetType() == cmTarget::STATIC_LIBRARY) ||
       (t->second.GetType() == cmTarget::SHARED_LIBRARY) ||
       (t->second.GetType() == cmTarget::MODULE_LIBRARY))
      {
      if(t->second.IsInAll())
        {
        std::string dep = this->GetTargetDirectory(t->second);
        dep += "/";
        dep += t->first;
        dep += ".depends";
        depends.push_back(dep);
        }
      }
    }
  this->WriteMakeRule(makefileStream,
                      "Main dependencies target for this directory.",
                      0,
                      "all.depends",
                      depends,
                      commands);
  }

  // Output top level build rule.
  {
  std::vector<std::string> depends;
  std::vector<std::string> commands;
  for(cmTargets::const_iterator t = targets.begin(); t != targets.end(); ++t)
    {
    // TODO: Dispatch generation of each target type.
    if((t->second.GetType() == cmTarget::EXECUTABLE) ||
       (t->second.GetType() == cmTarget::STATIC_LIBRARY) ||
       (t->second.GetType() == cmTarget::SHARED_LIBRARY) ||
       (t->second.GetType() == cmTarget::MODULE_LIBRARY))
      {
      if(t->second.IsInAll())
        {
        depends.push_back(t->first+".requires");
        }
      }
    }
  this->WriteMakeRule(makefileStream,
                      "Main build target for this directory.",
                      0,
                      "all.build",
                      depends,
                      commands);
  }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteRequiresRule(std::ostream& ruleFileStream, const cmTarget& target,
                    const char* targetFullPath)
{
  std::vector<std::string> depends;
  std::vector<std::string> no_commands;
  std::string reqComment = "Requirements for target ";
  reqComment += target.GetName();
  std::string reqTarget = target.GetName();
  reqTarget += ".requires";
  depends.push_back(targetFullPath);
  this->WriteMakeRule(ruleFileStream, reqComment.c_str(), 0,
                      reqTarget.c_str(), depends, no_commands);
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteTargetIncludes(std::ostream& makefileStream)
{
  bool first = true;
  const cmTargets& targets = m_Makefile->GetTargets();
  for(cmTargets::const_iterator t = targets.begin(); t != targets.end(); ++t)
    {
    // TODO: Handle the rest of the target types.
    if((t->second.GetType() == cmTarget::EXECUTABLE) ||
       (t->second.GetType() == cmTarget::STATIC_LIBRARY) ||
       (t->second.GetType() == cmTarget::SHARED_LIBRARY) ||
       (t->second.GetType() == cmTarget::MODULE_LIBRARY))
      {
      // Write the header for this section.
      if(first)
        {
        this->WriteDivider(makefileStream);
        makefileStream
          << "# Include rule files for each target in this directory.\n"
          << "\n";
        first = false;
        }

      // Construct the rule file name for this target.
      std::string ruleFileName = this->GetTargetDirectory(t->second);
      ruleFileName += "/";
      ruleFileName += t->first;
      ruleFileName += ".make";
      makefileStream
        << m_IncludeDirective << " "
        << this->ConvertToOutputForExisting(ruleFileName.c_str()).c_str()
        << "\n";
      }
    }
  if(!first)
    {
    makefileStream << "\n";
    }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteExecutableRule(std::ostream& ruleFileStream,
                      const char* ruleFileName,
                      const cmTarget& target,
                      std::vector<std::string>& objects)
{
  std::vector<std::string> commands;

  // Build list of dependencies.  TODO: depend on other targets.
  std::vector<std::string> depends;
  for(std::vector<std::string>::const_iterator obj = objects.begin();
      obj != objects.end(); ++obj)
    {
    depends.push_back(*obj);
    }

  // Add dependencies on libraries that will be linked.
  std::set<cmStdString> emitted;
  emitted.insert(target.GetName());
  const cmTarget::LinkLibraries& tlibs = target.GetLinkLibraries();
  for(cmTarget::LinkLibraries::const_iterator lib = tlibs.begin();
      lib != tlibs.end(); ++lib)
    {
    // Don't emit the same library twice for this target.
    if(emitted.insert(lib->first).second)
      {
      // Add this dependency.
      this->AppendLibDepend(depends, lib->first.c_str());
      }
    }
  depends.push_back(ruleFileName);

  // Construct the full path to the executable that will be generated.
  std::string targetFullPath = m_ExecutableOutputPath;
  if(targetFullPath.length() == 0)
    {
    targetFullPath = m_Makefile->GetStartOutputDirectory();
    if(targetFullPath.size() && targetFullPath[targetFullPath.size()-1] != '/')
      {
      targetFullPath += "/";
      }
    }
#ifdef __APPLE__
  if(target.GetPropertyAsBool("MACOSX_BUNDLE"))
    {
    // Make bundle directories
    targetFullPath += target.GetName();
    targetFullPath += ".app/Contents/MacOS/";
    }
#endif
  targetFullPath += target.GetName();
  targetFullPath += cmSystemTools::GetExecutableExtension();
  targetFullPath = this->ConvertToRelativeOutputPath(targetFullPath.c_str());

  // Get the language to use for linking this executable.
  const char* linkLanguage =
    target.GetLinkerLanguage(this->GetGlobalGenerator());

  // Build a list of compiler flags and linker flags.
  std::string flags;
  std::string linkFlags;

  // Add flags to create an executable.
  this->AddConfigVariableFlags(linkFlags, "CMAKE_EXE_LINKER_FLAGS");
  if(target.GetPropertyAsBool("WIN32_EXECUTABLE"))
    {
    this->AppendFlags(linkFlags,
                      m_Makefile->GetDefinition("CMAKE_CREATE_WIN32_EXE"));
    }
  else
    {
    this->AppendFlags(linkFlags,
                      m_Makefile->GetDefinition("CMAKE_CREATE_CONSOLE_EXE"));
    }

  // Add language-specific flags.
  this->AddLanguageFlags(flags, linkLanguage);

  // Add flags to deal with shared libraries.  Any library being
  // linked in might be shared, so always use shared flags for an
  // executable.
  this->AddSharedFlags(flags, linkLanguage, true);

  // Add target-specific linker flags.
  this->AppendFlags(linkFlags, target.GetProperty("LINK_FLAGS"));

  // TODO: Pre-build and pre-link rules.

  // Construct the main link rule.
  std::string linkRuleVar = "CMAKE_";
  linkRuleVar += linkLanguage;
  linkRuleVar += "_LINK_EXECUTABLE";
  std::string linkRule = m_Makefile->GetRequiredDefinition(linkRuleVar.c_str());
  cmSystemTools::ExpandListArgument(linkRule, commands);

  // TODO: Post-build rules.

  // Collect up flags to link in needed libraries.
  cmOStringStream linklibs;
  this->OutputLinkLibraries(linklibs, 0, target);

  // Construct object file lists that may be needed to expand the
  // rule.  TODO: Store these in make variables with line continuation
  // to avoid excessively long lines.
  std::string objs;
  std::string objsQuoted;
  const char* space = "";
  for(std::vector<std::string>::iterator i = objects.begin();
      i != objects.end(); ++i)
    {
    objs += space;
    objs += this->ConvertToRelativeOutputPath(i->c_str());
    objsQuoted += space;
    objsQuoted += "\"";
    objsQuoted += this->ConvertToRelativeOutputPath(i->c_str());
    objsQuoted += "\"";
    space = " ";
    }

  // Expand placeholders in the commands.
  for(std::vector<std::string>::iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    this->ExpandRuleVariables(*i,
                              linkLanguage,
                              objs.c_str(),
                              targetFullPath.c_str(),
                              linklibs.str().c_str(),
                              0,
                              0,
                              flags.c_str(),
                              0,
                              0,
                              0,
                              linkFlags.c_str());
    }

  // Write the build rule.
  std::string buildEcho = "Linking ";
  buildEcho += linkLanguage;
  buildEcho += " executable ";
  buildEcho += this->ConvertToRelativeOutputPath(targetFullPath.c_str());
  buildEcho += "...";
  this->WriteMakeRule(ruleFileStream, 0, buildEcho.c_str(),
                      targetFullPath.c_str(), depends, commands);
  // TODO: Add "local" target and canonical target name as rules.

  // Write driver rule for this target.
  this->WriteRequiresRule(ruleFileStream, target, targetFullPath.c_str());
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteStaticLibraryRule(std::ostream& ruleFileStream,
                         const char* ruleFileName,
                         const cmTarget& target,
                         std::vector<std::string>& objects)
{
  const char* linkLanguage =
    target.GetLinkerLanguage(this->GetGlobalGenerator());
  std::string linkRuleVar = "CMAKE_";
  linkRuleVar += linkLanguage;
  linkRuleVar += "_CREATE_STATIC_LIBRARY";

  std::string extraFlags;
  this->AppendFlags(extraFlags, target.GetProperty("STATIC_LIBRARY_FLAGS"));
  this->WriteLibraryRule(ruleFileStream, ruleFileName, target, objects,
                         linkRuleVar.c_str(), extraFlags.c_str());
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteSharedLibraryRule(std::ostream& ruleFileStream,
                         const char* ruleFileName,
                         const cmTarget& target,
                         std::vector<std::string>& objects)
{
  const char* linkLanguage =
    target.GetLinkerLanguage(this->GetGlobalGenerator());
  std::string linkRuleVar = "CMAKE_";
  linkRuleVar += linkLanguage;
  linkRuleVar += "_CREATE_SHARED_LIBRARY";

  std::string extraFlags;
  this->AppendFlags(extraFlags, target.GetProperty("LINK_FLAGS"));
  this->AddConfigVariableFlags(extraFlags, "CMAKE_SHARED_LINKER_FLAGS");
  if(m_Makefile->IsOn("WIN32") && !(m_Makefile->IsOn("CYGWIN") || m_Makefile->IsOn("MINGW")))
    {
    const std::vector<cmSourceFile*>& sources = target.GetSourceFiles();
    for(std::vector<cmSourceFile*>::const_iterator i = sources.begin();
        i != sources.end(); ++i)
      {
      if((*i)->GetSourceExtension() == "def")
        {
        extraFlags += " ";
        extraFlags += m_Makefile->GetSafeDefinition("CMAKE_LINK_DEF_FILE_FLAG");
        extraFlags += this->ConvertToRelativeOutputPath((*i)->GetFullPath().c_str());
        }
      }
    }
  this->WriteLibraryRule(ruleFileStream, ruleFileName, target, objects,
                         linkRuleVar.c_str(), extraFlags.c_str());
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteModuleLibraryRule(std::ostream& ruleFileStream,
                         const char* ruleFileName,
                         const cmTarget& target,
                         std::vector<std::string>& objects)
{
  const char* linkLanguage =
    target.GetLinkerLanguage(this->GetGlobalGenerator());
  std::string linkRuleVar = "CMAKE_";
  linkRuleVar += linkLanguage;
  linkRuleVar += "_CREATE_SHARED_MODULE";

  std::string extraFlags;
  this->AppendFlags(extraFlags, target.GetProperty("LINK_FLAGS"));
  this->AddConfigVariableFlags(extraFlags, "CMAKE_MODULE_LINKER_FLAGS");
  // TODO: .def files should be supported here also.
  this->WriteLibraryRule(ruleFileStream, ruleFileName, target, objects,
                         linkRuleVar.c_str(), extraFlags.c_str());
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteLibraryRule(std::ostream& ruleFileStream,
                   const char* ruleFileName,
                   const cmTarget& target,
                   std::vector<std::string>& objects,
                   const char* linkRuleVar,
                   const char* extraFlags)
{
  // TODO: Merge the methods that call this method to avoid
  // code duplication.
  std::vector<std::string> commands;

  // Build list of dependencies.  TODO: depend on other targets.
  std::vector<std::string> depends;
  for(std::vector<std::string>::const_iterator obj = objects.begin();
      obj != objects.end(); ++obj)
    {
    depends.push_back(*obj);
    }
  depends.push_back(ruleFileName);

  const char* linkLanguage =
    target.GetLinkerLanguage(this->GetGlobalGenerator());
  std::string linkFlags;
  this->AppendFlags(linkFlags, extraFlags);
  std::string targetName;
  std::string targetNameSO;
  std::string targetNameReal;
  std::string targetNameBase;
  this->GetLibraryNames(target.GetName(), target,
                        targetName, targetNameSO,
                        targetNameReal, targetNameBase);

  std::string outpath;
  std::string outdir;
  if(m_UseRelativePaths)
    {
    outdir = this->ConvertToRelativeOutputPath(m_LibraryOutputPath.c_str());
    }
  else
    {
    outdir = m_LibraryOutputPath;
    }
  if(!m_WindowsShell && m_UseRelativePaths && outdir.size())
    {
    outpath =  "\"`cd ";
    }
  outpath += outdir;
  if(!m_WindowsShell && m_UseRelativePaths && outdir.size())
    {
    outpath += ";pwd`\"/";
    }
  if(outdir.size() == 0 && m_UseRelativePaths && !m_WindowsShell)
    {
    outpath = "\"`pwd`\"/";
    }
  // The full path versions of the names.
  std::string targetFullPath = outpath + targetName;
  std::string targetFullPathSO = outpath + targetNameSO;
  std::string targetFullPathReal = outpath + targetNameReal;
  std::string targetFullPathBase = outpath + targetNameBase;
  // If not using relative paths then the output path needs to be
  // converted here
  if(!m_UseRelativePaths)
    {
    targetFullPath = this->ConvertToRelativeOutputPath(targetFullPath.c_str());
    targetFullPathSO = this->ConvertToRelativeOutputPath(targetFullPathSO.c_str());
    targetFullPathReal = this->ConvertToRelativeOutputPath(targetFullPathReal.c_str());
    targetFullPathBase = this->ConvertToRelativeOutputPath(targetFullPathBase.c_str());
    }

  // Add a command to remove any existing files for this library.
  std::string remove = "$(CMAKE_COMMAND) -E remove -f ";
  remove += targetFullPathReal;
  if(targetFullPathSO != targetFullPathReal)
    {
    remove += " ";
    remove += targetFullPathSO;
    }
  if(targetFullPath != targetFullPathSO &&
     targetFullPath != targetFullPathReal)
    {
    remove += " ";
    remove += targetFullPath;
    }
  commands.push_back(remove);

  // TODO: Pre-build and pre-link rules.

  // Construct the main link rule.
  std::string linkRule = m_Makefile->GetRequiredDefinition(linkRuleVar);
  cmSystemTools::ExpandListArgument(linkRule, commands);

  // Add a rule to create necessary symlinks for the library.
  if(targetFullPath != targetFullPathReal)
    {
    std::string symlink = "$(CMAKE_COMMAND) -E cmake_symlink_library ";
    symlink += targetFullPathReal;
    symlink += " ";
    symlink += targetFullPathSO;
    symlink += " ";
    symlink += targetFullPath;
    commands.push_back(symlink);
    }

  // TODO: Post-build rules.

  // Collect up flags to link in needed libraries.
  cmOStringStream linklibs;
  this->OutputLinkLibraries(linklibs, target.GetName(), target);

  // Construct object file lists that may be needed to expand the
  // rule.  TODO: Store these in make variables with line continuation
  // to avoid excessively long lines.
  std::string objs;
  std::string objsQuoted;
  const char* space = "";
  for(std::vector<std::string>::iterator i = objects.begin();
      i != objects.end(); ++i)
    {
    objs += space;
    objs += this->ConvertToRelativeOutputPath(i->c_str());
    objsQuoted += space;
    objsQuoted += "\"";
    objsQuoted += this->ConvertToRelativeOutputPath(i->c_str());
    objsQuoted += "\"";
    space = " ";
    }

  // Expand placeholders in the commands.
  for(std::vector<std::string>::iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    this->ExpandRuleVariables(*i,
                              linkLanguage,
                              objs.c_str(),
                              targetFullPathReal.c_str(),
                              linklibs.str().c_str(),
                              0, 0, 0, objsQuoted.c_str(),
                              targetFullPathBase.c_str(),
                              targetNameSO.c_str(),
                              linkFlags.c_str());
    }

  // Write the build rule.
  std::string buildEcho = "Linking ";
  buildEcho += linkLanguage;
  switch(target.GetType())
    {
    case cmTarget::STATIC_LIBRARY:
      buildEcho += " static library "; break;
    case cmTarget::SHARED_LIBRARY:
      buildEcho += " shared library "; break;
    case cmTarget::MODULE_LIBRARY:
      buildEcho += " shared module "; break;
    default:
      buildEcho += " library "; break;
    }
  buildEcho += this->ConvertToRelativeOutputPath(targetFullPath.c_str());
  buildEcho += "...";
  this->WriteMakeRule(ruleFileStream, 0, buildEcho.c_str(),
                      targetFullPath.c_str(), depends, commands);

  // TODO: Add "local" target and canonical target name as rules.

  // Write driver rule for this target.
  this->WriteRequiresRule(ruleFileStream, target, targetFullPath.c_str());
}

//----------------------------------------------------------------------------
std::string
cmLocalUnixMakefileGenerator2
::GetTargetDirectory(const cmTarget& target)
{
  std::string dir = target.GetName();
  dir += ".dir";
  return dir;
}

//----------------------------------------------------------------------------
std::string
cmLocalUnixMakefileGenerator2
::GetObjectFileName(const cmTarget& target,
                    const cmSourceFile& source)
{
  // If the full path to the source file includes this directory,
  // we want to use the relative path for the filename of the
  // object file.  Otherwise, we will use just the filename
  // portion.
  std::string objectName;
  if((cmSystemTools::GetFilenamePath(
        source.GetFullPath()).find(
          m_Makefile->GetCurrentDirectory()) == 0)
     || (cmSystemTools::GetFilenamePath(
           source.GetFullPath()).find(
             m_Makefile->GetStartOutputDirectory()) == 0))
    {
    objectName = source.GetSourceName();
    }
  else
    {
    objectName = cmSystemTools::GetFilenameName(source.GetSourceName());
    }

  // Append the object file extension.
  objectName +=
    m_GlobalGenerator->GetLanguageOutputExtensionFromExtension(
      source.GetSourceExtension().c_str());

  // Convert to a safe name.
  objectName = this->CreateSafeUniqueObjectFileName(objectName.c_str());

  // Prepend the target directory.
  std::string obj = this->GetTargetDirectory(target);
  obj += "/";
  obj += objectName;
  return obj;
}

//----------------------------------------------------------------------------
const char*
cmLocalUnixMakefileGenerator2
::GetSourceFileLanguage(const cmSourceFile& source)
{
  // Identify the language of the source file.
  return (m_GlobalGenerator
          ->GetLanguageFromExtension(source.GetSourceExtension().c_str()));
}

//----------------------------------------------------------------------------
std::string
cmLocalUnixMakefileGenerator2
::ConvertToFullPath(const std::string& localPath)
{
  std::string dir = m_Makefile->GetStartOutputDirectory();
  dir += "/";
  dir += localPath;
  return dir;
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::AddLanguageFlags(std::string& flags,
                                                     const char* lang)
{
  // Add language-specific flags.
  std::string flagsVar = "CMAKE_";
  flagsVar += lang;
  flagsVar += "_FLAGS";
  this->AddConfigVariableFlags(flags, flagsVar.c_str());
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::AddSharedFlags(std::string& flags,
                                                   const char* lang,
                                                   bool shared)
{
  std::string flagsVar;

  // Add flags for dealing with shared libraries for this language.
  if(shared)
    {
    flagsVar = "CMAKE_SHARED_LIBRARY_";
    flagsVar += lang;
    flagsVar += "_FLAGS";
    this->AppendFlags(flags, m_Makefile->GetDefinition(flagsVar.c_str()));
    }

  // Add flags specific to shared builds.
  if(cmSystemTools::IsOn(m_Makefile->GetDefinition("BUILD_SHARED_LIBS")))
    {
    flagsVar = "CMAKE_SHARED_BUILD_";
    flagsVar += lang;
    flagsVar += "_FLAGS";
    this->AppendFlags(flags, m_Makefile->GetDefinition(flagsVar.c_str()));
    }
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::AddConfigVariableFlags(std::string& flags,
                                                           const char* var)
{
  // Add the flags from the variable itself.
  std::string flagsVar = var;
  this->AppendFlags(flags, m_Makefile->GetDefinition(flagsVar.c_str()));

  // Add the flags from the build-type specific variable.
  const char* buildType = m_Makefile->GetDefinition("CMAKE_BUILD_TYPE");
  if(buildType && *buildType)
    {
    flagsVar += "_";
    flagsVar += cmSystemTools::UpperCase(buildType);
    this->AppendFlags(flags, m_Makefile->GetDefinition(flagsVar.c_str()));
    }
}

//----------------------------------------------------------------------------
void cmLocalUnixMakefileGenerator2::AppendFlags(std::string& flags,
                                                const char* newFlags)
{
  if(newFlags && *newFlags)
    {
    if(flags.size())
      {
      flags += " ";
      }
    flags += newFlags;
    }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::AppendLibDepend(std::vector<std::string>& depends, const char* name)
{
  // There are a few cases for the name of the target:
  //  - CMake target in this directory: depend on it.
  //  - CMake target in another directory: depend and add jump-and-build.
  //  - Full path to an outside file: depend on it.
  //  - Other format (like -lm): do nothing.

  // If it is a CMake target there will be a definition for it.
  std::string dirVar = name;
  dirVar += "_CMAKE_PATH";
  const char* dir = m_Makefile->GetDefinition(dirVar.c_str());
  if(dir && *dir)
    {
    // This is a CMake target somewhere in this project.
    bool jumpAndBuild = false;

    // Get the path to  the library.
    std::string libPath;
    if(this->SamePath(m_Makefile->GetStartOutputDirectory(), dir))
      {
      // The target is in the current directory so this makefile will
      // know about it already.
      libPath = m_LibraryOutputPath;
      }
    else
      {
      // The target is in another directory.  Get the path to it.
      if(m_LibraryOutputPath.size())
        {
        libPath = m_LibraryOutputPath;
        }
      else
        {
        libPath = dir;
        libPath += "/";
        }

      // We need to add a jump-and-build rule for this library.
      jumpAndBuild = true;
      }

    // Add the name of the library's file.  This depends on the type
    // of the library.
    std::string typeVar = name;
    typeVar += "_LIBRARY_TYPE";
    std::string libType = m_Makefile->GetSafeDefinition(typeVar.c_str());
    std::string prefix;
    std::string suffix;
    if(libType == "SHARED")
      {
      prefix = m_Makefile->GetSafeDefinition("CMAKE_SHARED_LIBRARY_PREFIX");
      suffix = m_Makefile->GetSafeDefinition("CMAKE_SHARED_LIBRARY_SUFFIX");
      }
    else if(libType == "MODULE")
      {
      prefix = m_Makefile->GetSafeDefinition("CMAKE_SHARED_MODULE_PREFIX");
      suffix = m_Makefile->GetSafeDefinition("CMAKE_SHARED_MODULE_SUFFIX");
      }
    else if(libType == "STATIC")
      {
      prefix = m_Makefile->GetSafeDefinition("CMAKE_STATIC_LIBRARY_PREFIX");
      suffix = m_Makefile->GetSafeDefinition("CMAKE_STATIC_LIBRARY_SUFFIX");
      }
    libPath += prefix;
    libPath += name;
    libPath += suffix;

    if(jumpAndBuild)
      {
      // We need to add a jump-and-build rule for this library.
      cmLocalUnixMakefileGenerator2::RemoteTarget rt;
      rt.m_BuildDirectory = dir;
      rt.m_FilePath =libPath;
      m_JumpAndBuild[name] = rt;
      }

    // Add a dependency on the library.
    depends.push_back(this->ConvertToRelativeOutputPath(libPath.c_str()));
    }
  else
    {
    // This is not a CMake target.  If it exists and is a full path we
    // can depend on it.
    if(cmSystemTools::FileExists(name) && cmSystemTools::FileIsFullPath(name))
      {
      depends.push_back(this->ConvertToRelativeOutputPath(name));
      }
    }
}

//----------------------------------------------------------------------------
std::string
cmLocalUnixMakefileGenerator2
::GetRecursiveMakeCall(const char* tgt)
{
  // Call make on the given file.
  std::string cmd = "$(MAKE) -f Makefile2 ";

  // Pass down verbosity level.
  if(m_MakeSilentFlag.size())
    {
    cmd += m_MakeSilentFlag;
    cmd += " ";
    }

  // Most unix makes will pass the command line flags to make down to
  // sub-invoked makes via an environment variable.  However, some
  // makes do not support that, so you have to pass the flags
  // explicitly.
  if(m_PassMakeflags)
    {
    cmd += "-$(MAKEFLAGS) ";
    }

  // Add the target.
  cmd += tgt;

  return cmd;
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteJumpAndBuildRules(std::ostream& makefileStream)
{
  // Write the header for this section.
  if(!m_JumpAndBuild.empty())
    {
    this->WriteDivider(makefileStream);
    makefileStream
      << "# Targets to make sure needed libraries exist.\n"
      << "# These will jump to other directories to build targets.\n"
      << "\n";
    }

  std::vector<std::string> depends;
  std::vector<std::string> commands;
  for(std::map<cmStdString, RemoteTarget>::iterator
        jump = m_JumpAndBuild.begin(); jump != m_JumpAndBuild.end(); ++jump)
    {
    const cmLocalUnixMakefileGenerator2::RemoteTarget& rt = jump->second;
    const char* destination = rt.m_BuildDirectory.c_str();

    // Construct the dependency and build target names.
    std::string dep = jump->first;
    dep += ".dir/";
    dep += jump->first;
    dep += ".depends";
    dep = this->ConvertToRelativeOutputPath(dep.c_str());
    std::string tgt = jump->first;
    tgt += ".requires";
    tgt = this->ConvertToRelativeOutputPath(tgt.c_str());

    // Build the jump-and-build command list.
    commands.clear();
    if(m_WindowsShell)
      {
      // On Windows we must perform each step separately and then jump
      // back because the shell keeps the working directory between
      // commands.
      std::string cmd = "cd ";
      cmd += this->ConvertToOutputForExisting(destination);
      commands.push_back(cmd);

      // Check the build system in destination directory.
      commands.push_back(this->GetRecursiveMakeCall("cmake_check_build_system"));

      // Build the targets's dependencies.
      commands.push_back(this->GetRecursiveMakeCall(dep.c_str()));

      // Build the target.
      commands.push_back(this->GetRecursiveMakeCall(tgt.c_str()));

      // Jump back to the starting directory.
      cmd = "cd ";
      cmd += this->ConvertToOutputForExisting(m_Makefile->GetStartOutputDirectory());
      commands.push_back(cmd);
      }
    else
      {
      // On UNIX we must construct a single shell command to jump and
      // build because make resets the directory between each command.
      std::string cmd = "cd ";
      cmd += this->ConvertToOutputForExisting(destination);

      // Check the build system in destination directory.
      cmd += " && ";
      cmd += this->GetRecursiveMakeCall("cmake_check_build_system");

      // Build the targets's dependencies.
      cmd += " && ";
      cmd += this->GetRecursiveMakeCall(dep.c_str());

      // Build the target.
      cmd += " && ";
      cmd += this->GetRecursiveMakeCall(tgt.c_str());

      // Add the command as a single line.
      commands.push_back(cmd);
      }

    // Write the rule.
    std::string jumpPreEcho = "Jumping to ";
    jumpPreEcho += rt.m_BuildDirectory.c_str();
    jumpPreEcho += " to build ";
    jumpPreEcho += jump->first;
    jumpPreEcho += "...";
    std::string jumpPostEcho = "Returning to ";
    jumpPostEcho += m_Makefile->GetStartOutputDirectory();
    jumpPostEcho += "...";
    this->WriteMakeRule(makefileStream, 0, jumpPreEcho.c_str(),
                        rt.m_FilePath.c_str(), depends, commands,
                        jumpPostEcho.c_str());
    }
}

//----------------------------------------------------------------------------
bool
cmLocalUnixMakefileGenerator2
::ScanDependencies(std::vector<std::string> const& args)
{
  // Format of arguments is:
  // $(CMAKE_COMMAND), cmake_depends, <lang>, <obj>, <src>, [include-flags]
  // The caller has ensured that all required arguments exist.

  // The file to which to write dependencies.
  const char* objFile = args[3].c_str();

  // The source file at which to start the scan.
  const char* srcFile = args[4].c_str();

  // Convert the include flags to full paths.
  std::vector<std::string> includes;
  for(unsigned int i=5; i < args.size(); ++i)
    {
    if(args[i].substr(0, 2) == "-I")
      {
      // Get the include path without the -I flag.
      std::string inc = args[i].substr(2);
      includes.push_back(cmSystemTools::CollapseFullPath(inc.c_str()));
      }
    }

  // Dispatch the scan for each language.
  std::string const& lang = args[2];
  if(lang == "C" || lang == "CXX")
    {
    return cmLocalUnixMakefileGenerator2::ScanDependenciesC(objFile, srcFile,
                                                            includes);
    }
  return false;
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2ScanDependenciesC(
  std::ifstream& fin,
  std::set<cmStdString>& encountered,
  std::queue<cmStdString>& unscanned)
{
  // Regular expression to identify C preprocessor include directives.
  cmsys::RegularExpression
    includeLine("^[ \t]*#[ \t]*include[ \t]*[<\"]([^\">]+)[\">]");

  // Read one line at a time.
  std::string line;
  while(cmSystemTools::GetLineFromStream(fin, line))
    {
    // Match include directives.  TODO: Support include regex and
    // ignore regex.  Possibly also support directory-based inclusion
    // in dependencies.
    if(includeLine.find(line.c_str()))
      {
      // Get the file being included.
      std::string includeFile = includeLine.match(1);

      // Queue the file if it has not yet been encountered.
      if(encountered.find(includeFile) == encountered.end())
        {
        encountered.insert(includeFile);
        unscanned.push(includeFile);
        }
      }
    }
}

//----------------------------------------------------------------------------
bool
cmLocalUnixMakefileGenerator2
::ScanDependenciesC(const char* objFile, const char* srcFile,
                    std::vector<std::string> const& includes)
{
  // Walk the dependency graph starting with the source file.
  std::set<cmStdString> dependencies;
  std::set<cmStdString> encountered;
  std::set<cmStdString> scanned;
  std::queue<cmStdString> unscanned;
  unscanned.push(srcFile);
  encountered.insert(srcFile);
  while(!unscanned.empty())
    {
    // Get the next file to scan.
    std::string fname = unscanned.front();
    unscanned.pop();

    // If not a full path, find the file in the include path.
    std::string fullName;
    if(cmSystemTools::FileIsFullPath(fname.c_str()))
      {
      fullName = fname;
      }
    else
      {
      for(std::vector<std::string>::const_iterator i = includes.begin();
          i != includes.end(); ++i)
        {
        std::string temp = *i;
        temp += "/";
        temp += fname;
        if(cmSystemTools::FileExists(temp.c_str()))
          {
          fullName = temp;
          break;
          }
        }
      }

    // Scan the file if it was found and has not been scanned already.
    if(fullName.size() && (scanned.find(fullName) == scanned.end()))
      {
      // Record scanned files.
      scanned.insert(fullName);

      // Try to scan the file.  Just leave it out if we cannot find
      // it.
      std::ifstream fin(fullName.c_str());
      if(fin)
        {
        // Add this file as a dependency.
        dependencies.insert(fullName);

        // Scan this file for new dependencies.
        cmLocalUnixMakefileGenerator2ScanDependenciesC(fin, encountered,
                                                       unscanned);
        }
      }
    }

  // Write the dependencies to the output file.
  std::string depMarkFile = objFile;
  std::string depMakeFile = objFile;
  depMarkFile += ".depends";
  depMakeFile += ".depends.make";
  std::ofstream fout(depMakeFile.c_str());
  fout << "# Dependencies for " << objFile << std::endl;
  for(std::set<cmStdString>::iterator i=dependencies.begin();
      i != dependencies.end(); ++i)
    {
    fout << objFile << ": "
         << cmSystemTools::ConvertToOutputPath(i->c_str()).c_str()
         << std::endl;
    }
  fout << std::endl;
  fout << "# Dependencies for " << depMarkFile.c_str() << std::endl;
  for(std::set<cmStdString>::iterator i=dependencies.begin();
      i != dependencies.end(); ++i)
    {
    fout << depMarkFile.c_str() << ": "
         << cmSystemTools::ConvertToOutputPath(i->c_str()).c_str()
         << std::endl;
    }

  // If we could write the dependencies, touch the corresponding
  // depends file to mark dependencies up to date.
  if(fout)
    {
    std::ofstream fmark(depMarkFile.c_str());
    fmark << "Dependencies updated for " << objFile << "\n";
    }

  return true;
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::CheckDependencies(const char* depCheck)
{
  // Get the list of files to scan.  This is given through the command
  // line hook cmake file.
  std::vector<std::string> files;
  cmSystemTools::ExpandListArgument(depCheck, files);

  // Check each file.  The current working directory is already
  // correct.
  for(std::vector<std::string>::iterator f = files.begin();
      f != files.end(); ++f)
    {
    cmLocalUnixMakefileGenerator2::CheckDependencies(".", f->c_str());
    }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::CheckDependencies(const char* dir, const char* file)
{
  // Check the dependencies associated with the given file whose path
  // is specified relative to the given directory.  If any dependency
  // is missing then dependencies should be regenerated.
  bool regenerate = false;

  // Construct the names of the mark and make files.
  std::string depMarkFileFull = dir;
  depMarkFileFull += "/";
  depMarkFileFull += file;
  depMarkFileFull += ".depends";
  std::string depMakeFileFull = depMarkFileFull;
  depMakeFileFull += ".make";

  // Open the dependency makefile.
  std::ifstream fin(depMakeFileFull.c_str());
  if(fin)
    {
    // Parse dependencies.
    std::string line;
    std::string depender;
    std::string dependee;
    while(cmSystemTools::GetLineFromStream(fin, line))
      {
      // Skip empty lines and comments.
      std::string::size_type pos = line.find_first_not_of(" \t\r\n");
      if(pos == std::string::npos || line[pos] == '#')
        {
        continue;
        }

      // Strip leading whitespace.
      if(pos > 0)
        {
        line = line.substr(pos);
        }

      // Skip lines too short to have a dependency.
      if(line.size() < 2)
        {
        continue;
        }

      // Find the colon on the line.  Skip the first two characters to
      // avoid finding the colon in a drive letter on Windows.  Ignore
      // the line if a colon cannot be found.
      if((pos = line.find(':', 2)) == std::string::npos)
        {
        continue;
        }

      // Split the line into depender and dependee.
      depender = line.substr(0, pos);
      dependee = line.substr(pos+1);

      // Strip whitespace from the dependee.
      if((pos = dependee.find_first_not_of(" \t\r\n")) != std::string::npos &&
         pos > 0)
        {
        dependee = dependee.substr(pos);
        }
      if((pos = dependee.find_last_not_of(" \t\r\n")) != std::string::npos)
        {
        dependee = dependee.substr(0, pos+1);
        }

      // Convert dependee to a full path.
      if(!cmSystemTools::FileIsFullPath(dependee.c_str()))
        {
        dependee = cmSystemTools::CollapseFullPath(dependee.c_str(), dir);
        }

      // If the dependee does not exist, we need to regenerate
      // dependencies and the depender should be removed.
      if(!cmSystemTools::FileExists(dependee.c_str()))
        {
        // Strip whitespace from the depender.
        if((pos = depender.find_last_not_of(" \t\r\n")) != std::string::npos)
          {
          depender = depender.substr(0, pos+1);
          }

        // Convert depender to a full path.
        if(!cmSystemTools::FileIsFullPath(depender.c_str()))
          {
          depender = cmSystemTools::CollapseFullPath(depender.c_str(), dir);
          }

        // Remove the depender.
        cmSystemTools::RemoveFile(depender.c_str());

        // Mark the need for regeneration.
        regenerate = true;
        }
      }
    }
  else
    {
    // Could not open the dependencies file.  It needs to be
    // regenerated.
    regenerate = true;
    }

  // If the dependencies file needs to be regenerated, create an empty
  // one and delete the mark file.
  if(regenerate)
    {
    cmLocalUnixMakefileGenerator2
      ::WriteEmptyDependMakeFile(file, depMarkFileFull.c_str(),
                                 depMakeFileFull.c_str());
    }
}

//----------------------------------------------------------------------------
void
cmLocalUnixMakefileGenerator2
::WriteEmptyDependMakeFile(const char* file,
                           const char* depMarkFileFull,
                           const char* depMakeFileFull)
{
  // Remove the dependency mark file to be sure dependencies will be
  // regenerated.
  cmSystemTools::RemoveFile(depMarkFileFull);

  // Write an empty dependency file.
  std::ofstream depFileStream(depMakeFileFull);
  depFileStream
    << "# Empty dependencies file for " << file << ".\n"
    << "# This may be replaced when dependencies are built.\n";
}
