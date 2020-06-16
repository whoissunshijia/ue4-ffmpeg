// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
public class Uffmpeg : ModuleRules
{
    private string ModulePath
    {
        get { return ModuleDirectory; }
    }
    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty")); }
    }
    private string ProjectPath
    {
        get { return Directory.GetParent(ModulePath).Parent.FullName; }
    }

    public Uffmpeg(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {

            string PlatformString = (Target.Platform == UnrealTargetPlatform.Win64) ? "x64" : "Win32";
            string LibrariesPath = Path.Combine(Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "lib"), "vs"), PlatformString);    

            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avcodec.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avdevice.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avfilter.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avformat.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avutil.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "swresample.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "swscale.lib"));

            string[] dlls = { "avcodec-58.dll", "avdevice-58.dll", "avfilter-7.dll", "avformat-58.dll", "avutil-56.dll", "swresample-3.dll", "swscale-5.dll", "postproc-55.dll" };

            string BinariesPath = Path.Combine(Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "bin"), "vs"), PlatformString);
            System.Console.WriteLine("... LibrariesPath -> " + BinariesPath);
            foreach (string dll in dlls)
            {
                PublicDelayLoadDLLs.Add(dll);
                RuntimeDependencies.Add(Path.Combine(BinariesPath, dll), StagedFileType.NonUFS);
            }

        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string LibrariesPath = Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "lib"), "osx");

            System.Console.WriteLine("... LibrariesPath -> " + LibrariesPath);

            string[] libs = { "libavcodec.58.dylib", "libavdevice.58.dylib", "libavfilter.7.dylib", "libavformat.58.dylib", "libavutil.56.dylib", "libswresample.3.dylib", "libswscale.5.dylib", "libpostproc.55.dylib" };
            foreach (string lib in libs)
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, lib));
                RuntimeDependencies.Add(Path.Combine(LibrariesPath, lib), StagedFileType.NonUFS);
            }

        }
        // Include path
        PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "ffmpeg", "include"));
        PublicIncludePaths.Add(Path.Combine(Directory.GetCurrentDirectory(), "Runtime","AudioMixer","Private"));


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AudioMixer",
				// ... add other public dependencies that you statically link with here ...
			}
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Projects",
                "Engine",
                "RHI",
                "UnrealEd"
				// ... add private dependencies that you statically link with here ...	
			}
            );


        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        //bEnableUndefinedIdentifierWarnings = false;

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );

    }
}
