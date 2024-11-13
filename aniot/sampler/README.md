# Cuestión

- ¿Qué prioridad tiene la tarea inicial que ejecuta la función app_main()? ¿Con qué llamada de ESP-IDF podemos conocer la prioridad de una tarea?

La prioridad que tiene la tarea inicial la cual ejecuta la función app_main() es 1. Para saber la prioridad de una tarea se puede utilizar la función uxTaskPriorityGet().

- ¿Cómo sincronizas ambas tareas? ¿Cómo sabe la tarea inicial que hay un nuevo dato generado por la tarea muestreadora?

Para sincronizar ambas tareas, se puede definir y utilizar un semáforo binario. La tarea inicial sabrá que hay un nuevo dato generado en la tarea muestreadora porque esta le envía una notificación que confirma la generación de un dato, el cual el semáforo estará esperando.
Existen muchas otras formas también.

- Si además de pasar el período como parámetro, quisiéramos pasar como argumento la dirección en la que la tarea muestreadora debe escribir las lecturas, ¿cómo pasaríamos los dos argumentos a la nueva tarea?

Para enviar dos argumentos a una misma tarea, se define una estructura de variables el cual contiene la información de las variables que se vayan a utilizar. Posteriormente, cuando se crea la tarea, se asocia el puntero de la estructura de variables.

# Cuestión

- Al enviar un dato por una cola, ¿el dato se pasa por copia o por referencia?. Consulta la documentación para responder.
Cuando se envía un dato por una cola, la queue copia los datos y crea una versión independiente a la de los datos iniciales para evitar problemas de incongruencia.

# Cuestión

- ¿Qué debe hacer la tarea inicial tras registrar el handle? ¿Puede finalizar?

Cuanta la tarea inicial registre el handle, la tarea debe entrar en modo bucle a la espera de otras tareas que se han configurado. Por lo tanto, la tarea inicial no puede finalizar tras el registro del handle.
