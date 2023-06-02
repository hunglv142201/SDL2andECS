#include "ECS.h"
#include "SDL2/SDL.h"
#include <iostream>
#include <stdlib.h>

SDL_Renderer* renderer;

class Color
{
  public:
    int r;
    int g;
    int b;
};

class Transform
{
  public:
    float x;
    float y;
    int w;
    int h;
    Color color;
};

class Physic
{
  public:
    float velocity;
};

class RenderSystem : public ISystem
{
    void process(float deltaTime, std::vector<Entity>& entities, ECS& ecs) override
    {

        for (Entity entity : entities)
        {
            Transform* transform = ecs.getComponent<Transform>(entity);

            SDL_Rect rect = {static_cast<int>(transform->x), static_cast<int>(transform->y), transform->h,
                             transform->w};

            SDL_SetRenderDrawColor(renderer, transform->color.r, transform->color.g, transform->color.b, 0);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    Signature getSignature() override
    {
        Signature signature;
        signature.set(getComponentTypeId<Transform>());

        return signature;
    };
};

class PhysicSystem : public ISystem
{
    void process(float deltaTime, std::vector<Entity>& entities, ECS& ecs) override
    {
        for (Entity entity : entities)
        {
            Transform* transform = ecs.getComponent<Transform>(entity);
            Physic* physic = ecs.getComponent<Physic>(entity);

            transform->y += physic->velocity * deltaTime;
        }
    }

    Signature getSignature() override
    {
        Signature signature;
        signature.set(getComponentTypeId<Transform>());
        signature.set(getComponentTypeId<Physic>());

        return signature;
    };
};

void initECS(ECS& ecs)
{
    ecs.registerComponent<Transform>();
    ecs.registerComponent<Physic>();

    ecs.registerSystem(new RenderSystem());
    ecs.registerSystem(new PhysicSystem());

    for (int i = 0; i < MAX_ENTITIES; i++)
    {
        Transform transform;
        transform.x = rand() % 640;
        transform.y = rand() % 640;
        transform.w = 32;
        transform.h = 32;
        Color color;
        color.r = rand() % 255;
        color.g = rand() % 255;
        color.b = rand() % 255;
        transform.color = color;

        Physic physic;
        physic.velocity = rand() % 80 + 20;

        Entity entity = ecs.newEntity();
        ecs.assignComponent(entity, transform);
        ecs.assignComponent(entity, physic);
    }
}

int main(int argv, char** args)
{
    ECS ecs;
    initECS(ecs);

    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_Window* window =
        SDL_CreateWindow("ECS Testing", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 640, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 0);

    const int FPS = 60;
    const int TICKS_PER_FRAME = 1000 / FPS;

    int frameStart = 0;
    int ticks = 0;

    while (true)
    {
        frameStart = SDL_GetTicks();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        ecs.processSystem(ticks / 1000.0f);

        ticks = SDL_GetTicks() - frameStart;

        if (ticks < TICKS_PER_FRAME)
        {
            SDL_Delay(TICKS_PER_FRAME - ticks);
        }

        ticks = SDL_GetTicks() - frameStart;

        SDL_RenderPresent(renderer);

        std::cout << 1000.0f / ticks << "\n";
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
