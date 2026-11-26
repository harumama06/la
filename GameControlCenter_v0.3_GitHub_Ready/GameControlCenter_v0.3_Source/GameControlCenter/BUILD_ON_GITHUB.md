# GitHub Actions build (current nested repository layout)

The repository root contains the workflow and this source folder:

- Workflow: `.github/workflows/windows-build.yml` at the repository root
- C++ project: `GameControlCenter_v0.3_Source/GameControlCenter/`

The workflow checks the project path, checks out vcpkg with full history for the manifest baseline, builds Release x64 on Windows Server 2022, deploys Qt runtime files, and uploads `GameControlCenter-v0.3-Windows-x64.zip` as an artifact.

## Run it
1. Preserve the folder structure when uploading this package to the GitHub repository.
2. Open **Actions** → **Game Control Center Windows Build**.
3. Select **Run workflow**, or commit/push to `main` or `master` to trigger it.
4. When the run succeeds, download the artifact `GameControlCenter-v0.3-Windows-x64` from the run's Artifacts section.
5. Extract the ZIP and keep the entire portable folder together. Run `GameControlCenter.exe`.

A first Qt build may take a long time. The workflow has not been verified on your current GitHub run until the Actions job completes successfully.
