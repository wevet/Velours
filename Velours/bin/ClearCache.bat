rmdir /q /s ..\\Intermediate
rmdir /q /s ..\\Saved\\Autosaves
rmdir /q /s ..\\Saved\\Backup
rmdir /q /s ..\\Saved\\Collections
rmdir /q /s ..\\Saved\\Config
rmdir /q /s ..\\Saved\\Crashes
rmdir /q /s ..\\Saved\\Logs

rmdir /q /s ..\\.vs
rmdir /q /s ..\\Binaries
rmdir /q /s ..\\DerivedDataCache

rmdir /q /s ..\\Plugins\\ProcHitReact\\Binaries
rmdir /q /s ..\\Plugins\\ProcHitReact\\Intermediate

rmdir /q /s ..\\Plugins\\BrokenGlassEffects\\Binaries
rmdir /q /s ..\\Plugins\\BrokenGlassEffects\\Intermediate
rmdir /q /s ..\\Plugins\\QuadrupedIK\\Binaries
rmdir /q /s ..\\Plugins\\QuadrupedIK\\Intermediate
rmdir /q /s ..\\Plugins\\WvAbilitySystem\\Binaries
rmdir /q /s ..\\Plugins\\WvAbilitySystem\\Intermediate
rmdir /q /s ..\\Plugins\\WvPostProcess\\Binaries
rmdir /q /s ..\\Plugins\\WvPostProcess\\Intermediate
rmdir /q /s ..\\Plugins\\WvPhysics\\Binaries
rmdir /q /s ..\\Plugins\\WvPhysics\\Intermediate

del ..\\.vsconfig
del ..\\Velours.sln

pause
exit 0
