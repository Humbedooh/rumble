/* Rumble_version.h */
#ifndef RUMBLE_VERSION_H
#   define RUMBLE_VERSION_H
#   define RUMBLE_VERSION_REQUIRED           0x002F0000   /* Internal version for module checks */
#   define RUMBLE_VERSION                    0x00310C1D   /* Application version */
#   define RUMBLE_MAJOR             (RUMBLE_VERSION & 0xFF000000) >> 24
#   define RUMBLE_MINOR             (RUMBLE_VERSION & 0x00FF0000) >> 16
#   define RUMBLE_REV               (RUMBLE_VERSION & 0x0000FFFF)
#endif /* RUMBLE_VERSION_H */
