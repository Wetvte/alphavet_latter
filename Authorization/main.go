package main

import (
	"net/http"

	"github.com/gorilla/mux"
)

func HomeHandler(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("Добро пожаловать в сервис авторизации!"))
}

func main() {
	router := mux.NewRouter()
	router.HandleFunc("/", HomeHandler)
	
	http.ListenAndServe(":8080", router)
}
