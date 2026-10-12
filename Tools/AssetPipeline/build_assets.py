"""
Builds Cosmo Chomp's Unreal assets from the raw files in Assets/.

    python Tools/AssetPipeline/build_assets.py

Steps:
  1. ffmpeg      Assets/sounds/**/*.mp3      -> Intermediate/AssetStaging/sounds/*.wav (UE can't import MP3)
  2. trimesh     simple-ray-gun model.dae    -> Intermediate/AssetStaging/raygun.glb (Blender 5 has no Collada importer)
  3. Blender     blender_prep.py             -> splits the wall FBX into pieces, converts rocket + ray gun to FBX
  4. Unreal      ue_import.py (commandlet)   -> imports everything into /Game and builds materials

Requirements: ffmpeg on PATH, Blender 5.x, Unreal Engine 5.8, and `pip install trimesh pycollada`.
Override tool locations with the BLENDER_EXE / UE_EDITOR_CMD environment variables.
Close the Unreal Editor before running, otherwise the commandlet can't save the assets.
"""
import glob
import os
import shutil
import subprocess
import sys
import zipfile

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TOOLS_DIR, "..", ".."))
ASSETS_DIR = os.path.join(PROJECT_DIR, "Assets")
STAGING_DIR = os.path.join(PROJECT_DIR, "Intermediate", "AssetStaging")

BLENDER_EXE = os.environ.get("BLENDER_EXE", r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")
UE_EDITOR_CMD = os.environ.get("UE_EDITOR_CMD", r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")

# Assets/sounds/<folder> -> /Game/Audio/<asset name>
SOUNDS = {
    "bang": "S_Bang",
    "claw-attack": "S_ClawAttack",
    "footsteps": "S_Footsteps",
    "gun-shoot": "S_GunShoot",
    "ough": "S_Ough",
    "rocket-launch": "S_RocketLaunch",
    "slime": "S_Slime",
    "whoosh": "S_Whoosh",
}


def run(cmd):
    print(">", " ".join(f'"{c}"' if " " in c else c for c in cmd), flush=True)
    subprocess.run(cmd, check=True)


def convert_sounds():
    out_dir = os.path.join(STAGING_DIR, "sounds")
    os.makedirs(out_dir, exist_ok=True)
    for folder, name in SOUNDS.items():
        src = glob.glob(os.path.join(ASSETS_DIR, "sounds", folder, "*.mp3"))[0]
        run(["ffmpeg", "-v", "error", "-y", "-i", src, "-ar", "44100", "-sample_fmt", "s16", os.path.join(out_dir, name + ".wav")])


def convert_raygun():
    try:
        import trimesh  # noqa: F401  (pycollada is needed by trimesh's DAE loader)
    except ImportError:
        sys.exit("trimesh is missing: run `pip install trimesh pycollada`")
    import trimesh

    extract_dir = os.path.join(STAGING_DIR, "raygun_src")
    with zipfile.ZipFile(os.path.join(ASSETS_DIR, "models", "simple-ray-gun", "source", "model.zip")) as z:
        z.extractall(extract_dir)
    scene = trimesh.load(os.path.join(extract_dir, "model", "model.dae"), force="scene")
    scene.export(os.path.join(STAGING_DIR, "raygun.glb"))
    print("wrote raygun.glb", flush=True)


def stage_rigged_models():
    # The rigged FBX files import directly; copy them under the asset names we want in UE
    shutil.copy(os.path.join(ASSETS_DIR, "models", "cute-astronaut", "source", "Astronaut.fbx"),
                os.path.join(STAGING_DIR, "SK_Astronaut.fbx"))
    shutil.copy(os.path.join(ASSETS_DIR, "models", "one-eyed-monster-with-animations", "source",
                             "Claymation monster with animations.fbx"),
                os.path.join(STAGING_DIR, "SK_Alien.fbx"))


def main():
    os.makedirs(STAGING_DIR, exist_ok=True)
    convert_sounds()
    convert_raygun()
    stage_rigged_models()
    run([BLENDER_EXE, "-b", "--python", os.path.join(TOOLS_DIR, "blender_prep.py"), "--", PROJECT_DIR, STAGING_DIR])
    run([UE_EDITOR_CMD, os.path.join(PROJECT_DIR, "CosmoChomp.uproject"), "-run=pythonscript",
         "-script=" + os.path.join(TOOLS_DIR, "ue_import.py").replace("\\", "/"), "-unattended", "-nop4", "-nosplash", "-stdout"])


if __name__ == "__main__":
    main()
