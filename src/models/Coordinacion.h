#ifndef COORDINACION_H
#define COORDINACION_H

#include "Usuario.h"

class Coordinacion : public Usuario {
public:
    Coordinacion(int id, std::string n, std::string a, std::string c, std::string pass)
        : Usuario(id, n, a, c, pass, "Coordinacion") {}

    // Methods to manage users would likely interact with the Database manager
    // rather than being implemented purely here in the model
};

#endif // COORDINACION_H
