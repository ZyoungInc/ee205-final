

#include "player.h"
#include "vector/vec2.h"
#include "collision.h"
#include "level/region.h"
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>


Player::Player(sf::View& view, sf::View& hud)
: view(view)
, hud(hud)
, box({64, 64})
, box_g({64, 64})
, body_hb(32)
, foot_hb({16, 16})
{

    if(!font.loadFromFile("FiraCode-VF.ttf")) {
        PRINT("unable to load font");
    }

    l_pos.setFont(font);
    l_pos.setCharacterSize(14);
    l_pos.setColor(sf::Color::White);
    l_pos.setOrigin({600, +320});

    l_vel.setFont(font);
    l_vel.setCharacterSize(14);
    l_vel.setColor(sf::Color::Green);
    l_vel.setOrigin({600, +320 - 16});

    l_plat.setFont(font);
    l_plat.setCharacterSize(14);
    l_plat.setColor(sf::Color::White);
    l_plat.setOrigin({600, +320 - 64});

    l_region.setFont(font);
    l_region.setCharacterSize(14);
    l_region.setColor(sf::Color::White);
    l_region.setOrigin({0, +320});

    tri.setPointCount(3);
    tri.setPoint(0, {0, 0});
    tri.setPoint(1, {64, 0});
    tri.setPoint(2, {0, 64});
    tri.setOrigin({32, 32});
    // tri.setFillColor({150, 150, 150});

    box.setOrigin({32, 32});
    box_g.setOrigin({32, 32});
    box_g.setFillColor({255, 255, 255, 0});

    body_hb.setFillColor(sf::Color::Transparent);
    body_hb.setOutlineColor(sf::Color::Transparent);
    // body_hb.setOutlineColor(sf::Color::Red);
    body_hb.setOutlineThickness(2.0f);
    body_hb.setOrigin({32, 32});

    foot_hb.setFillColor(sf::Color::Transparent);
    foot_hb.setOutlineColor(sf::Color::Transparent);
    // foot_hb.setOutlineColor(sf::Color::Red);
    foot_hb.setOutlineThickness(2.0f);
    foot_hb.setOrigin({8, 8 - 40});

    add_child(box);
    add_child(tri);
    add_child(body_hb);
    add_child(foot_hb);
    add_child_free(box_g);

    set_color({255, 255, 255});
}


void Player::set_color(sf::Color color) {

    sf::Color darker(color.r * 0.8, color.g * 0.8, color.b * 0.8);

    this->box.setFillColor(color);
    this->tri.setFillColor(darker);
}


void Player::key_press(Key key) {

    //=========================================================================
    // DEBUG RESET BUTTON
    //=========================================================================

    if(key == Key::R)
        return this->respawn();

    //=========================================================================
    // LEFT / RIGHT MOVEMENT
    //=========================================================================

    if(key == Key::D)
        r_pressed = 1;

    if(key == Key::A)
        l_pressed = 1;

    //=========================================================================
    // JUMP
    //=========================================================================

    if(key == Key::I and (stasis or can_jump) and not is_spinning)
        this->do_jump();

    //=========================================================================
    // DASH / SPIN
    //=========================================================================

    if(key == Key::U) {

        is_spinning = true;

        // set_color({255, 255, 150});

        using key = Key;

        float x = 0, y = 0;

        if(Keyboard::isKeyPressed(Key::D)) {
            x = 1;
        }
        else if(Keyboard::isKeyPressed(Key::A)) {
            x = -1;
        }

        if(Keyboard::isKeyPressed(Key::S)) {
            y = 1;
        }
        else if(Keyboard::isKeyPressed(Key::W)) {
            y = -1;
        }

        if(x != 0 or y != 0) {
            vec2 dir = vec2(x, y).normalize();
            this->do_dash(dir);
        }
    }
}

void Player::key_release(Key key) {

    if(key == Key::D)
        r_pressed = 0;

    if(key == Key::A)
        l_pressed = 0;

    if(key == Key::U) {
        PRINT("unspinning");
        is_spinning = false;
    }
}


void Player::do_jump() {

    vel.y = -P_JUMP_VEL;

    move({0, -1}); // shift player slightly up to avoid ground collisions

    jump_timer = 0.2f;
    stasis = false;

    // jump smoke effect

    for(int i = -32; i <= 32; i += 16) {

        Particle* part = new Particle(this);
        part->handle->setPosition(this->get_pos() + vec2(i, 40));
        this->add_child_free((Entity*)part);

    }
    PRINT("jump");
}


void Player::do_dash(const vec2& dir) {

    if(num_dashes > 0) {

        if(can_jump) move({0, -1});

        num_dashes--;

        this->vel = dir * P_DASH_VEL;
        show_afterimage();

        if(num_dashes <= 0) {
            set_color(P_COLOR_NODASH);
        }
    }
}


void Player::do_move(Direction dir, float delta) {

    switch(dir) {
    case LEFT:

        if(vel.x > -P_WALK_MAX_VEL)
            vel.x -= delta * P_WALK_ACCEL;
        break;

    case RIGHT:

        if(vel.x < P_WALK_MAX_VEL)
            vel.x += delta * P_WALK_ACCEL;
        break;

    // x-axis deceleration due to no movement
    case Direction::NONE: default:

        if(!is_spinning) {

            if(vel.x > +0.5) { vel.x -= fmin(+delta * P_WALK_DECEL, vel.x); }
            if(vel.x < -0.5) { vel.x -= fmax(-delta * P_WALK_DECEL, vel.x); }

            if(vel.x <= 0.5 and vel.x >= -0.5) vel.x = 0;
        }
    }
}


Player::CollisionSet* Player::test_collisions() {

    // auto* cmap = new CollisionMap();
    auto* cset = new CollisionSet();

    if(!level) return cset;

    // PRINT("# cols => " + STR(cset->size()));

    Collision body_collision, foot_collision;

    for(Platform& line: level->platforms) {

        bool can_jump = false;
        SurfaceType col_type = NONE;
        vec2 normal;
        float angle = 0.f;
        float magnitude = 0.f;

        // PRINT("checking distance");

        float dist = (box.getPosition() - line.getPosition()).magnitude();

        // only test collision if within proximity
        if((box.getPosition() - line.getPosition()).magnitude() < line.get_length() + 10) {

            // PRINT("finding collision");

            // COLLISION WITH PLAYER
            // ---------------------

            body_collision = get_collision(line.get_shape(), body_hb);

            // PRINT("testing collision");

            // COLLISION WITH FEET HITBOX
            // --------------------------

            if(not can_jump) {

                foot_collision = get_collision(line.get_shape(), foot_hb);
                can_jump = foot_collision.has_collided();
            }

            if(body_collision.has_collided()) { // player is colliding

                // PRINT("collision found");

                // adjust object position by minimum translation vector

                normal = *body_collision.get_normal();
                magnitude = *body_collision.get_overlap_len();
                angle = normal.heading(-180, 180);


                if(angle >= -50 and angle <= 50) {

                    col_type = FLOOR;
                }
                else if(angle <= -135 or angle >= 135) {

                    col_type = CEILING;
                }
                else {

                    col_type = WALL;
                }

                // add collision info to the corresponding collision type
                // (*cmap)[col_type].push_back({can_jump, col_type, line, normal, angle, magnitude});
                // PRINT(
                //     "adding collision (" + STR(cset->size()) + ")\n"
                //     "  surface: " + STR(col_type) + "\n"
                //     "  normal: " + (string)normal + "\n"
                //     "  angle: " + STR(angle) + "\n"
                //     "  mag: " + STR(magnitude) + "\n"
                // );
                // PRINT("plat_type => " + STR(line.get_type()));
                cset->insert({can_jump, col_type, &line, normal, angle, magnitude});
            }
        }
    }

    return cset;
}

void Player::update(float delta) {

    update_afterimage(delta);

    // ------------------------------------------------------------------------
    // camera calculations
    // ------------------------------------------------------------------------

    view.setSize({WIDTH / view_zoom, HEIGHT / view_zoom});

    if(!region or !region->is_inside()) {

        delete region;

        region = new LevelRegion(this, &view);
    }
    else {

        region->update_view(delta);
        l_region.setString(level->get_region_title(region->get_coords()));
    }

    // ------------------------------------------------------------------------
    // stasis behavior (initial behavior)
    // ------------------------------------------------------------------------

    if(stasis) {

        this->box.rotate(delta * P_STASIS_RVEL);
        this->tri.rotate(delta * P_STASIS_RVEL);

        // slowly zoom in
        if(view_zoom < 4.f)
            view_zoom = fmin(4.f, view_zoom + delta);

        return;
    }

    // zoom out
    if(view_zoom > 1.f)
        view_zoom = fmax(1.f, view_zoom - delta * 20);

    // PRINT("vel=" + (string)vel);
    // PRINT("cos=" << normal_cos);

    // ------------------------------------------------------------------------
    // gravity impl
    // ------------------------------------------------------------------------

    // y-axis acceleration due to gravity

    if(vel.y < P_GRAV_MAX and not is_grounded)
        vel.y += fmax(0.5, fmin(fabs(vel.y) / 1000, 1)) * delta * P_GRAV_ACCEL;

    Direction move_dir;

    if(Keyboard::isKeyPressed(P_KEY_LEFT) && !Keyboard::isKeyPressed(P_KEY_RIGHT)) {
        move_dir = LEFT;
    }
    else if(Keyboard::isKeyPressed(P_KEY_RIGHT) && !Keyboard::isKeyPressed(P_KEY_LEFT)) {
        move_dir = RIGHT;
    }
    else {
        move_dir = Direction::NONE;
    }

    this->do_move(move_dir, delta);

    // ==================================================
    //  collision checks
    // ==================================================

    vec2 pos = get_pos();

    // std::unique_ptr<CollisionMap> cmap(test_collisions());
    std::unique_ptr<CollisionSet> cset(test_collisions());

    // concat floor - wall - ceiling collisions, in that order

    bool can_jump = false;
    bool on_ground = false;

    string plat_info = "";


    float normal_cos = 1.f, normal_sin = 1.f;

    //-------------------------------------------------------------------------
    // check floor collisions

    // PRINT("# cols => " + STR(cset->size()));

    int i = 0;
    // for(CollisionInfo col: (*cmap)[FLOOR]) {
    for(auto col: *cset) {

        // PRINT("processing collision (" + STR(i) + ")");
        if(col.normal == vec2::ZERO) {
            PRINT(
                "WARNING: collided with a zero normal"
                // "  surface: " + STR(col.surface) + "\n"
                // "  normal: " + (string)col.normal + "\n"
                // "  angle: " + STR(col.angle) + "\n"
                // "  mag: " + STR(col.magnitude)
            );
            continue;
        }

        PlatformType plat_type = col.platform->get_type();

        // PRINT("plat_type => " + STR(plat_type));
        if(plat_type == PlatformType::HAZARD) {
            PRINT("==== PLAYER HIT HAZARD ====");
            this->respawn();
            return;
        }

        if(is_spinning) { // then bounce off the surface

            // calculates the angle of reflection
            this->vel = (vel - (col.normal * 2 * vel.dot(col.normal))) * P_BOUNCE_DAMP;
            break;
        }

        switch(col.surface) {
        case FLOOR:
            normal_cos = col.normal.dot(vec2::RIGHT);

            // only resolve collision in y-axis to avoid sliding on ramps
            // PRINT("surface = " + (string)col.normal);
            // PRINT("surface * UP = " + STR(col.normal.dot(vec2::UP)));
            pos.y -= col.magnitude / col.normal.dot(vec2::UP);

            plat_info += "FLOOR cos=" + STR(normal_cos) + "\n";

            if(!can_jump and col.can_jump) can_jump = true;
            if(!on_ground) on_ground = true;

            break;

    // }

    //-------------------------------------------------------------------------
    // check wall collisions

    // for(CollisionInfo col: (*cmap)[WALL]) {

        case WALL:
            // const PlatformType& plat_type = col.platform.get_type();
            // if(check_hazard_cols(col) or check_spinbounce(col)) break;

            normal_sin = col.normal.dot(vec2::UP);

            pos -= col.normal * col.magnitude; // resolve collision

            plat_info += "WALL sin=" + STR(normal_sin) + "\n";

            // wall: next position calculations

            break;


    //-------------------------------------------------------------------------
    // check ceiling collisions

    // for(CollisionInfo col: (*cmap)[CEILING]) {

        case CEILING:
        // const PlatformType& plat_type = col.platform.get_type();
        // if(check_hazard_cols(col) or check_spinbounce(col)) break;

        // resolve collisions

            pos -= col.normal * col.magnitude; // resolve collision
        // this->vel.y = 100;
            plat_info += "CEIL";

            break;
        }
        i++;
    }

    //-------------------------------------------------------------------------
    // position calculation in air (no collisions)

    pos.x += this->vel.x * fmax(normal_sin, 0.1) * delta;
    pos.y += this->vel.y * normal_cos * delta;

    if(jump_timer > 0)
        jump_timer -= delta;

    if(this->can_jump != can_jump) {

        this->can_jump = can_jump;

        if(can_jump) {
            PRINT("can now jump");
        } else {
            PRINT("cannot jump");
        }
    }

    if(this->is_grounded != on_ground) {

        if(this->is_grounded = on_ground) {
            PRINT("touched ground");
            set_color({255, 255, 255});
            num_dashes = 1;
        } else {
            PRINT("left ground");
            if(!is_spinning and jump_timer <= 0) vel.y = 0;
        }
    }

    if(is_spinning) {

        float mag = vel.magnitude();

        this->box.rotate(mag * delta * 2);
        this->tri.rotate(mag * delta * 2);

    } else {

        this->box.setRotation(this->vel.x * 50 / 1000);
        this->tri.setRotation(this->vel.x * 50 / 1000);
    }

    // 64 px above center of player
    l_pos.setString((string)pos);

    // 32 px above l_pos
    l_vel.setString((string)vel);

    // 64 px above l_vel
    l_plat.setString(plat_info + "\nspinning = " + STR(is_spinning) + "\ncan jump = " + STR(can_jump)
                     + "\ndashes = " + STR(num_dashes));

    // PRINT((string)pos);
    set_pos(pos);

    CompositeEntity::update(delta);
}

void Player::respawn() {

    set_pos(level->get_checkpoint(this->region->get_coords()));
}
