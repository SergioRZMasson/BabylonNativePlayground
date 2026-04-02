# 3DS Max SDK References

This folder must contain the Autodesk 3DS Max SDK DLLs required to build the plugin.

## Required Files

Copy the following files from your 3DS Max 2026 installation:

| File | Source Path |
|------|-------------|
| `Autodesk.Max.dll` | `C:\Program Files\Autodesk\3ds Max 2026\Autodesk.Max.dll` |
| `ManagedServices.dll` | `C:\Program Files\Autodesk\3ds Max 2026\ManagedServices.dll` |

## Version

These DLLs must be from **3DS Max 2026** (v28.x / SDK version 22.2.x). Using DLLs from a different Max version will cause runtime errors.

## Note

These files are not included in source control as they are proprietary Autodesk binaries.
