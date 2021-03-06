
#include "common.h"
#include "vector/vec2.h"
#include "player.h"

#define REGION_WIDTH 2000
#define REGION_HEIGHT 1000

/**
 * @brief Represents a region of the level.
 *
 */
class LevelRegion {
private:

    vec2 coords, min, max, vmin, vmax;

    sf::View *view; // reference to the view used to render the world

    Player *player; // reference to the player

public:

    LevelRegion(Player *player, sf::View *view);

    const vec2& get_coords() const { return coords; }

    /**
     * @brief Returns true if the player is inside the bounds defined by
     * min and max.
     *
     * @return true if the player is inside the region
     * @return false if the player is outside the region
     */
    bool is_inside(void);

    /**
     * @brief Updates the camera view
     *
     * @param delta
     */
    void update_view(float delta);
};
