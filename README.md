# Favorite Levels

Geode mod scaffold for Geometry Dash.

Features:
- `Favorite Levels` button on the main menu
- `Add to Favorite` / `Remove Favorite` button on level info pages
- configurable `Auto Backup` settings for the favorites list

This project currently implements:
- a new `Favorites *` button on the main menu
- an `Add to Favorite` / `Remove Favorite` button on level pages
- `Auto Backup` settings in the mod settings menu
- JSON backups stored in the mod save folder

Build prerequisites on Windows:
- Visual Studio Build Tools with MSVC C++
- a configured Geode SDK profile via `geode config setup`

Build with Geode CLI:

```bash
geode build --ninja
```

Build without installing MSVC locally:
- create a GitHub repository and push this project there
- open the `Actions` tab in GitHub
- run the `Build Geode Mod` workflow, or just push a commit
- after the workflow finishes, download the artifact named `favorite-levels-win64`
- inside it will be the built `.geode` file for Windows

Workflow file:
- [build-geode.yml](</C:/projects/favorite levels/.github/workflows/build-geode.yml:1>)

Helper files:
- [build_mod.bat](</C:/projects/favorite levels/build_mod.bat:1>)
- [install_mod.bat](</C:/projects/favorite levels/install_mod.bat:1>)
- [.gitignore](</C:/projects/favorite levels/.gitignore:1>)
