# Game Control Center v0.3

This package is arranged for the repository root. Extract it and upload/replace its contents at the root of your GitHub repository.

The GitHub Actions workflow is already included at:
`.github/workflows/windows-build.yml`

The project is already placed at the path expected by the workflow:
`GameControlCenter_v0.3_Source/GameControlCenter/`

After committing the files, Actions runs automatically on `main` or `master` (or can be started manually). The workflow builds Release x64, deploys the Qt runtime, and uploads a portable ZIP artifact.

No workflow edits or extra configuration files are needed.
