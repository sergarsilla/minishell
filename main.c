/*-


 * main.c
 * Minishell C source
 * Shows how to use "obtain_order" input interface function.
 *
 * Copyright (c) 1993-2002-2019, Francisco Rosales <frosal@fi.upm.es>
 * Todos los derechos reservados.
 *
 * Publicado bajo Licencia de Proyecto Educativo Práctico
 * <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
 *
 * Queda prohibida la difusión total o parcial por cualquier
 * medio del material entregado al alumno para la realización
 * de este proyecto o de cualquier material derivado de este,
 * incluyendo la solución particular que desarrolle el alumno.
 *
 * DO NOT MODIFY ANYTHING OVER THIS LINE
 * THIS FILE IS TO BE MODIFIED
 */

#include <stddef.h> /* NULL */
#include <stdio.h>	/* setbuf, printf */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <string.h>
#include <pwd.h>
#include <dirent.h>

#define MAX_BUF 100
#define MAX_FORK 40

int status;
extern int obtain_order(); /* See parser.y for description */
extern char **environ;
int mandato_interno(char **);

int main(void)
{
	char ***argvv = NULL;
	int argvc = 0;
	char **argv = NULL;
	int argc;
	char *filev[3] = {NULL, NULL, NULL};
	int bg;
	int ret;
	int fd0, fd1, fd2;
	pid_t pid, bgpid;
	int original_stdin, original_stdout, original_stderr;
	char varPrompt[MAX_BUF] = "prompt=msh> ", varMypid[MAX_BUF], varBgpid[MAX_BUF], varStatus[MAX_BUF];

	putenv(varPrompt);
	sprintf(varMypid, "mypid=%d", getpid());
	putenv(varMypid);

	setbuf(stdout, NULL); /* Unbuffered */
	setbuf(stdin, NULL);

	signal(SIGINT, SIG_IGN); // Configuramos señales para ignorar
	signal(SIGQUIT, SIG_IGN);

	while (1)
	{
		fprintf(stderr, "%s", "msh> "); /* Prompt */
		ret = obtain_order(&argvv, filev, &bg);
		if (ret == 0)
			break; /* EOF */
		if (ret == -1)
			continue;	 /* Syntax error */
		argvc = ret - 1; /* Line */
		if (argvc == 0)
			continue; /* Empty line */

		if (filev[0])
		{
			fd0 = open(filev[0], O_RDONLY);
			if (fd0 < 0)
			{
				perror("El fichero < no se ha podido abrir con permisos de lectura\n");
				// exit(1);
				continue;
			}
			original_stdin = dup(STDIN_FILENO);
			dup2(fd0, STDIN_FILENO);
			close(fd0);
		}
		if (filev[1])
		{
			fd1 = creat(filev[1], 0666);
			if (fd1 < 0)
			{
				perror("El fichero > no se ha podido crear con permisos 0666\n");
				// exit(1);
				continue;
			}
			original_stdout = dup(STDOUT_FILENO);
			dup2(fd1, STDOUT_FILENO);
			close(fd1);
		}
		if (filev[2])
		{
			fd2 = creat(filev[2], 0666);
			if (fd2 < 0)
			{
				perror("El fichero >& no se ha podido crear con permisos 0666\n");
				// exit(1);
				continue;
			}
			original_stderr = dup(STDERR_FILENO);
			dup2(fd2, STDERR_FILENO);
			close(fd2);
		}
		int fd[argvc - 1][2]; // Array de pipes de longitud n_hijos - 1

		for (int i = 0; i < argvc; i++)
		{

			argv = argvv[i];
			int j = 0, err = 0; // err se usa para poder salir del bucle si hay errores
			while (argv[j] != NULL && err == 0)
			{
				if (argv[j][0] == '~')
				{ // Metacaracter ~[Usuario]
					char *user;
					user = malloc(sizeof(char) * MAX_BUF);
					struct passwd *passwd;
					if (sscanf(argv[j] + 1, "%[_a-zA-Z0-9]", user) > 0)
					{
						if ((passwd = getpwnam(user)) == NULL)
						{
							perror("Error en getpwnam\n");
							err++;
							continue;
						}
						if ((argv[j] = realloc(argv[j], strlen(passwd->pw_dir) + 1)) == NULL)
						{
							perror("Error en realloc\n");
							err++;
							continue;
						}
						strcpy(argv[j], passwd->pw_dir);
					}
					else
					{
						char *home = getenv("HOME");
						if (home == NULL)
						{
							perror("Error en getenv(\"HOME\")\n");
							err++;
							continue;
						}
						if ((argv[j] = realloc(argv[j], strlen(home) + 1)) == NULL)
						{
							perror("Error en realloc\n");
							err++;
							continue;
						}
						strcpy(argv[j], home);
					}
					free(user);
				}

				char var[MAX_BUF] = "", prefix[MAX_BUF] = "", *final;
				final = malloc(sizeof(char) * MAX_BUF);
				memset(final, 0, MAX_BUF); // Inicializamos los bytes de final a 0 para poder controlar si está vacío con strlen()
				int cont_final = 0, cont_argv = 0;

				while ((err == 0) && ((sscanf(argv[j] + cont_argv, "$%[_a-zA-Z0-9]", var) > 0 && strlen(var) > 0) || (sscanf(argv[j] + cont_argv, "%[^$]$%[_a-zA-Z0-9]", prefix, var) > 0 && strlen(var) > 0)))
				{ // Metacarácter $
					char *aux = getenv(var);
					if (aux == NULL)
					{
						perror("Error en getenv\n");
						err++;
						continue;
					}
					strncpy(final + cont_final, argv[j] + cont_argv, strlen(prefix)); // Copiamos el prefijo (si lo hay) en final y avanzamos punteros
					cont_final += strlen(prefix);
					cont_argv += strlen(prefix);

					strncpy(final + cont_final, aux, strlen(aux)); // Copiamos el valor de la variable guardado en aux en final y avanzamos puntero cont_final
					cont_final += strlen(aux);
					cont_argv += strlen(var) + 1; // Avanzamos puntero de argv para hacer el siguiente sscanf(), + 1 porque avanzamos el carácter $ que siempre ignoramos en sscanf()

					memset(prefix, 0, MAX_BUF);
					memset(var, 0, MAX_BUF); // Inicializamos a 0 prefix y var para el siguiente sscanf()
				}
				if (strlen(final) > 0)
				{
					strncpy(final + cont_final, prefix, strlen(prefix)); // Si hay algo en prefix significa que detrás de todas las variables hay por ejemplo un "./" y no se debe ignorar
					if ((argv[j] = realloc(argv[j], strlen(final) + 1)) == NULL)
					{
						perror("Error en realloc\n");
						err++;
						continue;
					}
					strcpy(argv[j], final);
				}

				free(final);

				char *comodin;
				if (strstr(argv[j], "/") == NULL && (comodin = strstr(argv[j], "?")) != NULL){ // Si no aparece / y aparece algún ?, se trata el carácter comodín
					DIR *d;
					struct dirent *dir;
					int k, iter_lista = 0, eq;
					char *lista[MAX_BUF];
					d = opendir(".");
					if (d != NULL){
						while((dir = readdir(d)) != NULL){
							k = 0;
							eq = 1;
							while(strlen(dir->d_name) == strlen(argv[j]) && dir->d_name[k] != '\0' && eq == 1){ // Bucle que comprueba si el siguiente nombre de archivo coincide con el formato especificado
								if (argv[j][k] != '?' && argv[j][k] != dir->d_name[k])
									eq = 0;
								
								k++;
							}

							if (eq == 1 && strlen(dir->d_name) == strlen(argv[j])){ // Si coincide, se guarda en el array
								lista[iter_lista] = dir->d_name;
								iter_lista++;
							}
						}
						
						if (iter_lista > 0 && (argv[j] = realloc(argv[j], strlen(lista[0]) * iter_lista + k + 1)) == NULL)
						{ // Hacer realloc del numero de elementos que hay en la lista, el + k indica el número de espacios que habrá
							perror("Error en realloc\n");
							err++;
							continue;
						}
						cont_argv = 0;
						for (k = iter_lista - 1; k >= 0; k--){ // Guardar los elementos de la lista con espacio entre ellos en argv[j]

							if (k == 0){
								strncpy(argv[j] + cont_argv, lista[k], strlen(lista[k]));
								cont_argv += strlen(lista[k]);
							}
							else{
								strncpy(argv[j] + cont_argv, lista[k], strlen(lista[k]));
								cont_argv += strlen(lista[k]);
								strncpy(argv[j] + cont_argv, " ", 2);
								cont_argv += 1;
							}
						}
						closedir(d);
					}
				}
				j++;
			}
			// if (err != 0) // Si ha habido un error dentro del bucle de metacaracteres no
			// 	continue;

			if (argvc > 1 && i < argvc - 1 && pipe(fd[i]) < 0)
			{ // En el último hijo no se crea ningún pipe
				perror("Error al crear el pipe\n");
				continue;
			}

			if (!bg && (argvc == 1 || i == argvc - 1)) // Si se invoca en foreground y no aparece en una secuencia o es el último de la secuencia se ejecuta mandato_interno en el proceso padre
			{
				if (mandato_interno(argv) == 1)
				{
					sprintf(varStatus, "status=%d", status); // Actualizamos la variable de entorno status aquí para no ejecutar el código
					putenv(varStatus);
					continue;
				}
			}

			pid = fork();
			switch (pid)
			{
			case -1:
				perror("Error al crear el proceso hijo\n");
				continue;
				break;
			case 0:
				if (!bg)
				{
					signal(SIGINT, SIG_DFL);  // Configuramos SIGINIT y SIGQUIT para que se comporten por defecto en la ejecución
					signal(SIGQUIT, SIG_DFL); // de mandatos en primer plano
				}
				else
				{
					signal(SIGINT, SIG_IGN);
					signal(SIGQUIT, SIG_IGN);
				}

				if (argvc > 1)
				{
					if (i != 0)
					{
						dup2(fd[i - 1][0], STDIN_FILENO);
						close(fd[i - 1][0]);
					}
					if (i != argvc - 1)
					{
						dup2(fd[i][1], STDOUT_FILENO);
						close(fd[i][1]);
					}
				}
				if (mandato_interno(argv) == 1) // Si coincide con un mandato interno no se realiza exec y se termina el proceso
					exit(status);

				// Mandato normal
				execvp(argv[0], argv);
				perror("La ejecución del mandato ha fallado\n");
				exit(1);

			default:
				if (!bg)
				{
					do
					{
						if (i == argvc - 1)
							ret = waitpid(pid, &status, 0);
						else
							ret = waitpid(pid, &status, WNOHANG);
					} while (ret > 0 && ret != pid);
				}
				else if (i == argvc - 1){
					bgpid = pid;
					fprintf(stdout, "[%d]\n", bgpid);
					
					sprintf(varBgpid, "bgpid=%d", bgpid);
					putenv(varBgpid);
				}
				
				if (argvc > 1)
				{
					if (i != 0)
						close(fd[i - 1][0]);

					if (i != argvc - 1)
						close(fd[i][1]);
				}
				signal(SIGINT, SIG_IGN);
				signal(SIGQUIT, SIG_IGN);
				break;
			}
		}

		if (filev[0])
		{
			dup2(original_stdin, STDIN_FILENO);
			close(original_stdin);
		}
		if (filev[1])
		{
			dup2(original_stdout, STDOUT_FILENO);
			close(original_stdout);
		}
		if (filev[2])
		{
			dup2(original_stderr, STDERR_FILENO);
			close(original_stderr);
		}

		if (!bg)
		{
			sprintf(varStatus, "status=%d", status);
			putenv(varStatus);
		}
	}
	exit(0);
	return 0;
}

int mandato_interno(char **argv)
{ // Devuelve 1 si argv coincide con un posible mandato interno y se ha ejecutado, y 0 si no coincide
  // Guarda en la variable global status el valor terminación del mandato
	char buf[MAX_BUF];
	if (strcmp(argv[0], "cd") == 0)
	{ // Mandato cd
		if (argv[1] == NULL)
		{
			char *home = getenv("HOME");
			if (chdir(home) == -1)
			{
				perror("No se ha podido cambiar el directorio\n");
				status = EXIT_FAILURE;
				return 1;
			}
		}
		else if (argv[2] == NULL)
		{
			if (chdir(argv[1]) == -1)
			{
				perror("No se ha podido cambiar el directorio\n");
				status = EXIT_FAILURE;
				return 1;
			}
		}
		else
		{
			perror("Demasiados argumentos\n");
			status = EXIT_FAILURE;
			return 1;
		}
		getcwd(buf, MAX_BUF);
		fprintf(stdout, "%s\n", buf);
		memset(buf, 0, MAX_BUF);
		status = EXIT_SUCCESS;
		return 1;
	}
	else if (strcmp(argv[0], "umask") == 0)
	{ // Mandato umask
		int mascara;
		if (argv[1] == NULL)
		{
			mascara = umask(0);
			umask(mascara);
		}
		else if (argv[2] == NULL)
		{
			char *endptr;
			mascara = strtol(argv[1], &endptr, 8);
			if (*endptr != '\0')
			{
				perror("El argumento debe ser un número en octal\n");
				status = EXIT_FAILURE;
				return 1;
			}
			mascara = umask(mascara);
		}
		else
		{
			perror("Demasiados argumentos\n");
			status = EXIT_FAILURE;
			return 1;
		}
		fprintf(stdout, "%o\n", mascara);
		status = EXIT_SUCCESS;
		return 1;
	}
	else if (strcmp(argv[0], "limit") == 0)
	{ // Mandato limit
		struct rlimit rlim;
		char *recurso[6] = {"cpu", "fsize", "data", "stack", "core", "nofile"};
		int j = 0, num_recurso[6] = {0, 1, 2, 3, 4, 7}; // El número de las macros RLIMIT_CPU... en orden
		if (argv[1] == NULL)
		{ // limit
			for (j = 0; j < 6; j++)
			{
				getrlimit(num_recurso[j], &rlim);
				fprintf(stdout, "%s\t%d\n", recurso[j], (int)rlim.rlim_cur); // Modificar, rlim[j].rlim_max
			}
		}
		else if (argv[2] == NULL)
		{ // limit [Recurso]
			while (j < 6 && strcmp(argv[1], recurso[j]) != 0)
				j++;
			if (j < 6)
			{
				getrlimit(num_recurso[j], &rlim);
				fprintf(stdout, "%s\t%d\n", recurso[j], (int)rlim.rlim_cur);
			}
			else
			{
				perror("Recurso no admitido: cpu, fsize, data, stack, core, nofile\n");
				status = EXIT_FAILURE;
				return 1;
			}
		}
		else if (argv[3] == NULL)
		{ // limit [Recurso] [Máximo]
			while (j < 6 && strcmp(argv[1], recurso[j]) != 0)
				j++;
			if (j < 6)
			{
				char *endptr;
				rlim.rlim_max = strtol(argv[2], &endptr, 10);
				rlim.rlim_cur = strtol(argv[2], &endptr, 10);
				// rlim.rlim_max = atoi(argv[2]);
				if (setrlimit(num_recurso[j], &rlim) == -1)
				{
					perror("No se ha podido establecer el límite correctamente\n");
					status = EXIT_FAILURE;
					return 1;
				}
			}
			else
			{
				perror("Recurso no admitido: cpu, fsize, data, stack, core, nofile\n");
				status = EXIT_FAILURE;
				return 1;
			}
		}
		else
		{
			perror("Demasiados argumentos\n");
			status = EXIT_FAILURE;
			return 1;
		}
		status = EXIT_SUCCESS;
		return 1;
	}
	else if (strcmp(argv[0], "set") == 0)
	{ // Mandato set
		if (argv[1] == NULL)
		{ // set
			int j = 0;
			while (environ[j])
			{
				fprintf(stdout, "%s\n", environ[j]);
				j++;
			}
		}
		else if (argv[2] == NULL)
		{ // set [Variable]
			char *valor = getenv(argv[1]);
			if (valor == NULL)
			{
				perror("La variable de entorno no está definida\n");
				status = EXIT_FAILURE;
				return 1;
			}
			else
				fprintf(stdout, "%s=%s\n", argv[1], valor);
		}
		else
		{ // set [Variable] [Valor...]
			int j = 3;
			char *var, *valor, *aux;
			var = malloc(sizeof(char) * MAX_BUF);
			valor = malloc(sizeof(char) * MAX_BUF);
			aux = malloc(sizeof(char) * MAX_BUF);

			strcpy(valor, argv[2]);
			while (argv[j] != NULL)
			{
				sprintf(aux, "%s %s", valor, argv[j]);
				strcpy(valor, aux);
				j++;
			}
			sprintf(var, "%s=%s", argv[1], valor);
			if (putenv(var) != 0)
			{
				perror("Error al definir la variable de entorno\n");
				status = EXIT_FAILURE;
				return 1;
			}
			free(valor);
			free(aux);
		}

		status = EXIT_SUCCESS;
		return 1;
	}
	return 0;
}