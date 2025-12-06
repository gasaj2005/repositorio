#ifndef USUARIO_H
#define USUARIO_H

#include <string>
#include <iostream>

class Usuario {
protected:
    int idUsuario;
    std::string nombre;
    std::string apellidos;
    std::string correo;
    std::string contrasena;
    std::string rol; // "Alumno", "Tutor", "Coordinacion"

public:
    Usuario(int id, std::string n, std::string a, std::string c, std::string pass, std::string r)
        : idUsuario(id), nombre(n), apellidos(a), correo(c), contrasena(pass), rol(r) {}
    
    virtual ~Usuario() = default;

    // Getters and Setters
    int getId() const { return idUsuario; }
    std::string getNombre() const { return nombre; }
    std::string getApellidos() const { return apellidos; }
    std::string getCorreo() const { return correo; }
    std::string getRol() const { return rol; }
    
    // Auth methods (placeholder for logic that might be in Controller)
    bool iniciarSesion(const std::string& email, const std::string& pass) {
        return (this->correo == email && this->contrasena == pass);
    }
    
    virtual void verPerfil() const {
        std::cout << "ID: " << idUsuario << "\nName: " << nombre << " " << apellidos << "\nRole: " << rol << std::endl;
    }
};

#endif // USUARIO_H
