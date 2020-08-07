// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class K4WLib : ModuleRules
{
    public K4WLib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string SDKDIR = Utils.ResolveEnvironmentVariable("%KINECTSDK20_DIR%");

        SDKDIR = SDKDIR.Replace("\\", "/");

        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            string BaseDirectory = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
            string RedistDirectory = Path.Combine(BaseDirectory, "ThirdParty/Redist");

            PublicIncludePaths.Add(SDKDIR + "inc/");
            string PlatformPath = (Target.Platform == UnrealTargetPlatform.Win64) ? "x64/" : "x86/";
            string LibPath = SDKDIR + "Lib/" + PlatformPath;

            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "Kinect20.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "Kinect20.Face.lib"));

            PublicDelayLoadDLLs.Add(Path.Combine(RedistDirectory, "Face", PlatformPath, "Kinect20.Face.dll"));
            RuntimeDependencies.Add("$(ProjectDir)/Binaries/Win64/Kinect20.Face.dll");
        }
    }
}
