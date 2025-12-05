package main

import (
	"net/http"

	"github.com/gorilla/mux"
)

func HomeHandler(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("Добро пожаловать в сервис авторизации!"))
}
func LoginHandler(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("Добро пожаловать на страницу авторизации!"))
}

func main() {
	router := mux.NewRouter()
	router.HandleFunc("/", HomeHandler)
	router.HandleFunc("/login", HomeHandler)
	
	http.ListenAndServe(":8080", router)
}
