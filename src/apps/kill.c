#include <kernel.h>

int
kill_main(int argc, char *argv[])
{
	if ( argc < 2 || argc > 3 )
	{
		cprintk(LIGHTRED, BLACK, "Uso: kill tarea [status]\n");
		return 1;
	}

	Task_t *task = (Task_t *) strtoul(argv[1], NULL, 16);
	int status = argc == 3 ? atoi(argv[2]) : 0;

	// Verificar que la tarea exista
	unsigned ntasks;
	TaskInfo_t *info;
	bool found;
	for ( found = false, info = GetTasks(&ntasks) ; ntasks-- ; info++ )
		if ( info->task == task )
		{
			found = true;
			break;
		}
	Free(info);

	if ( !found )
	{
		cprintk(LIGHTRED, BLACK, "Tarea inexistente\n");
		return 2;
	}

	if ( !DeleteTask(task, status) )
	{
		cprintk(LIGHTRED, BLACK, "Tarea protegida o terminando\n");
		return 3;
	}
	
	return 0;
}