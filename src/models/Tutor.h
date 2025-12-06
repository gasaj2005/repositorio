#ifndef TUTOR_H
#define TUTOR_H

#include "Usuario.h"
#include <vector>

class Tutor : public Usuario {
private:
    bool disponible;
    std::vector<int> listaAlumnosIds; // IDs of assigned students

public:
    Tutor(int id, std::string n, std::string a, std::string c, std::string pass, bool disp = true)
        : Usuario(id, n, a, c, pass, "Tutor"), disponible(disp) {}

    bool isDisponible() const { return disponible; }
    void setDisponible(bool d) { disponible = d; }
    
    void addAlumno(int alumnoId) {
        listaAlumnosIds.push_back(alumnoId);
    }

    const std::vector<int>& getAlumnos() const {
        return listaAlumnosIds;
    }
};

#endif // TUTOR_H
