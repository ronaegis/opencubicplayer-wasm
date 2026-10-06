/* generic-dialog-stepper.c - Implementation of generic dialog stepper framework
 *
 * See generic-dialog-stepper.h for usage documentation.
 *
 * Last updated: 2025-10-24
 */

#include "config.h"
#include "generic-dialog-stepper.h"
#include "stuff/poutput.h"
#include <string.h>
#include <stdio.h>

void generic_dialog_init(
	struct generic_dialog_engine *engine,
	const struct dialog_ops_t *ops,
	void *state,
	size_t state_size
)
{
	if (!engine || !ops || !state)
	{
		fprintf(stderr, "generic_dialog_init: NULL parameter\n");
		return;
	}

	memset(engine, 0, sizeof(*engine));
	engine->ops = ops;
	engine->state = state;
	engine->state_size = state_size;

	/* Clear the state structure */
	memset(state, 0, state_size);
}

int generic_dialog_run(
	struct generic_dialog_engine *engine,
	void **token,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit
)
{
	int iterations = 0;

	if (!engine || !engine->ops || !API)
	{
		fprintf(stderr, "generic_dialog_run: NULL parameter\n");
		return 0; /* Exit immediately */
	}

	/* First-time initialization */
	if (!engine->is_active)
	{
		engine->is_active = 1;
		engine->should_exit = 0;
		engine->first_frame = 1;

		if (engine->ops->init)
		{
			engine->ops->init(engine->state, token, API);
		}
	}

	/* Main event loop with iteration limit */
	while (!engine->should_exit &&
	       (iteration_limit < 0 || iterations < iteration_limit))
	{
		/* Draw the filesystem browser background */
		API->fsDraw();

		/* Draw the dialog */
		if (engine->ops->draw)
		{
			engine->ops->draw(engine->state, API);
		}

		/* Process all pending keyboard events */
		while (API->console->KeyboardHit() && !engine->should_exit)
		{
			int key = API->console->KeyboardGetChar();

			if (engine->ops->handle_key)
			{
				int result = engine->ops->handle_key(engine->state, key, API);
				if (result)
				{
					/* Dialog requested exit */
					engine->should_exit = 1;
					break;
				}
			}
		}

		/* Frame limiter - synchronize with display refresh */
		API->console->FrameLock();

		engine->first_frame = 0;
		iterations++;
	}

	/* Cleanup on exit */
	if (engine->should_exit)
	{
		if (engine->ops->cleanup)
		{
			engine->ops->cleanup(engine->state, token, API);
		}
		generic_dialog_reset(engine);
		return 0; /* Dialog exited */
	}

	return 1; /* Still running - call again next frame */
}

int generic_dialog_is_active(const struct generic_dialog_engine *engine)
{
	return engine ? engine->is_active : 0;
}

void generic_dialog_reset(struct generic_dialog_engine *engine)
{
	if (!engine)
	{
		return;
	}

	engine->is_active = 0;
	engine->should_exit = 0;
	engine->first_frame = 0;
	/* Note: We don't clear ops, state, or state_size - they may be reused */
}
