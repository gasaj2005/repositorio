#ifndef ALUMNO_H
#define ALUMNO_H

#include "Usuario.h"
#include <vector>

class Alumno : public Usuario {
private:
    std::string carrera;
    std::string historialAcademico;
    int tutorAsignadoId; // ID reference to Tutor

public:
    Alumno(int id, std::string n, std::string a, std::string c, std::string pass, std::string deg)
        : Usuario(id, n, a, c, pass, "Alumno"), carrera(deg), tutorAsignadoId(-1) {}

    void setTutor(int tutorId) { tutorAsignadoId = tutorId; }
    int getTutor() const { return tutorAsignadoId; }
    std::string getCarrera() const { return carrera; }
    
    void verPerfil() const override {
        Usuario::verPerfil();
        std::cout << "Degree: " << carrera << "\nTutor ID: " << tutorAsignadoId << std::endl;
    }
};

#endif // ALUMNO_H
