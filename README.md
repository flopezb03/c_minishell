En esta práctica se abordará el problema de implementar un programa que actúe como intérprete de mandatos. El minishell a implementar debe interpretar y ejecutar mandatos leyéndolos de la entrada estándar. En definitiva debe ser capaz de:

- Ejecutar una secuencia de uno o varios mandatos separados por el carácter ‘|’.
- Permitir redirecciones:
  - Entrada: < fichero. Sólo puede realizarse sobre el primer mandato del pipe.
  - Salida: > fichero. Sólo puede realizarse sobre el último mandato del pipe.
  - Error: >& fichero. Sólo puede realizarse sobre el último mandato del pipe.
- Permitir la ejecución en background de la secuencia de mandatos si termina con el carácter ‘&’. Para ello, el minishell debe mostrar el pid del proceso por el que espera entre corchetes, y no bloquearse por la ejecución de dicho mandato (es decir, no debe esperar a su terminación para mostrar el prompt).
