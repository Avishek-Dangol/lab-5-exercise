#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int FPS_LIMIT = 30;
const int TOTAL_FRAMES = 90;

using Point2D = sf::Vector2f;

Point2D mouseToPoint(sf::Vector2i position) {
    return Point2D{static_cast<float>(position.x), static_cast<float>(position.y)};
}
float lengthOf(Point2D v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// TODO: (Part 1) Define a function that samples a cubic Bezier curve at t in [0, 1].
Point2D getPoint(const std::vector<sf::Vector2f>& pts, float t) {
    if (pts.size() < 4) {
        return Point2D{0.f, 0.f};
    }
    // Cubic Bezier curve formula:
    const float u = 1.f - t;
    const float uu = u * u;
    const float uuu = uu * u;
    const float tt = t * t;
    const float ttt = tt * t;

    return Point2D{
        uuu * pts[0].x + 3.f * uu * t * pts[1].x + 3.f * u * tt * pts[2].x + ttt * pts[3].x,
        uuu * pts[0].y + 3.f * uu * t * pts[1].y + 3.f * u * tt * pts[2].y + ttt * pts[3].y};
}

// TODO: (Part 2) Define a function that returns the curve's slope at t in [0, 1].
Point2D getSlope(const std::vector<sf::Vector2f>& pts, float t) {
    if (pts.size() < 4) {
        return Point2D{0.f, 0.f};
    }
    const float u = 1.f - t;
    const Point2D a = pts[1] - pts[0];
    const Point2D b = pts[2] - pts[1];
    const Point2D c = pts[3] - pts[2];
    return 3.f * u * u * a + 6.f * u * t * b + 3.f * t * t * c;  // 3(1-t)² a + 6t(1-t) b + 3 t² c
}

std::vector<Point2D> segmentOf(const std::vector<Point2D>& curve, std::size_t start) {
    return {curve[start], curve[start + 1], curve[start + 2], curve[start + 3]};
}

// Line drawing from Project 1a DrawContext::DrawLine.
void drawLine(sf::RenderWindow& window, Point2D from, Point2D to, float width, sf::Color color) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length == 0.f) {
        return;
    }
    const float halfWidth = width / 2.f;
    sf::ConvexShape polygon(4);
    polygon.setPoint(0, {0.f, -halfWidth});
    polygon.setPoint(1, {length, -halfWidth});
    polygon.setPoint(2, {length, halfWidth});
    polygon.setPoint(3, {0.f, halfWidth});
    polygon.setFillColor(color);
    polygon.setPosition(from);
    polygon.setRotation(sf::radians(std::atan2(dy, dx)));
    window.draw(polygon);
}

void drawCurve(sf::RenderWindow& window, const std::vector<Point2D>& curve, sf::Color color) {
    constexpr int samples = 32;
    for (std::size_t start = 0; start + 3 < curve.size(); start += 3) {
        const std::vector<Point2D> segment = segmentOf(curve, start);
        Point2D previous = getPoint(segment, 0.f);
        for (int i = 1; i <= samples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(samples);
            const Point2D current = getPoint(segment, t);
            drawLine(window, previous, current, 2.f, color);
            previous = current;
        }
    }
}

void drawControlPoints(sf::RenderWindow& window, const std::vector<Point2D>& curve, int selected) {
    constexpr float radius = 8.f;
    for (std::size_t i = 0; i < curve.size(); ++i) {
        sf::CircleShape circle(radius);
        circle.setOrigin({radius, radius});
        circle.setPosition(curve[i]);
        if (static_cast<int>(i) == selected) {
            circle.setFillColor(sf::Color::Red);
        } else {
            circle.setFillColor(sf::Color::Cyan);
        }
        window.draw(circle);
    }
}

void drawHandles(sf::RenderWindow& window, const std::vector<Point2D>& curve) {
    for (std::size_t start = 0; start + 3 < curve.size(); start += 3) {
        drawLine(window, curve[start], curve[start + 1], 2.f, sf::Color::Yellow);
        drawLine(window, curve[start + 2], curve[start + 3], 2.f, sf::Color::Yellow);
    }
}

void drawOrientedSquare(sf::RenderWindow& window, const std::vector<Point2D>& curve, float time) {
    if (curve.size() < 4) {
        return;
    }
    const float count = static_cast<float>((curve.size() - 1) / 3);
    const float scaled = time * count;
    std::size_t segment = static_cast<std::size_t>(scaled);
    float localT = scaled - static_cast<float>(segment);
    if (segment >= static_cast<std::size_t>(count)) {
        segment = static_cast<std::size_t>(count) - 1;
        localT = 1.f;
    }
    const std::vector<Point2D> piece = segmentOf(curve, segment * 3);
    const Point2D slope = getSlope(piece, localT);
    constexpr float size = 14.f;
    sf::RectangleShape square({size, size});
    square.setOrigin({size * 0.5f, size * 0.5f});
    square.setPosition(getPoint(piece, localT));
    square.setRotation(sf::radians(std::atan2(slope.y, slope.x)));
    square.setFillColor(sf::Color::Green);
    window.draw(square);
}

void keepHandleSmooth(std::vector<Point2D>& curve, std::size_t moved) {
    if (moved % 3 == 2 && moved + 2 < curve.size()) {
        const std::size_t join = moved + 1;
        const std::size_t outgoing = moved + 2;
        const Point2D through = curve[join] - curve[moved];
        const float throughLength = lengthOf(through);
        if (throughLength < 0.0001f) {
            return;
        }
        const float keepDistance = lengthOf(curve[outgoing] - curve[join]);
        curve[outgoing] = curve[join] + (through / throughLength) * keepDistance;
        return;
    }
    if (moved >= 4 && moved % 3 == 1) {
        const std::size_t join = moved - 1;
        const std::size_t incoming = moved - 2;
        const Point2D through = curve[moved] - curve[join];
        const float throughLength = lengthOf(through);
        if (throughLength < 0.0001f) {
            return;
        }
        const float keepDistance = lengthOf(curve[incoming] - curve[join]);
        curve[incoming] = curve[join] - (through / throughLength) * keepDistance;
    }
}
void addThreePoints(std::vector<Point2D>& curve) {
    const Point2D end = curve.back();
    Point2D step = end - curve[curve.size() - 2];
    const float stepLength = lengthOf(step);
    if (stepLength < 0.0001f) {
        step = Point2D{1.f, 0.f};
    } else {
        step = step / stepLength;
    }
    const float handle = 80.f;
    const Point2D outgoing = end + step * handle;
    const Point2D nextControl = outgoing + Point2D{step.x * handle, step.y * handle + 70.f};
    const Point2D nextEnd = nextControl + step * handle;
    curve.push_back(outgoing);
    curve.push_back(nextControl);
    curve.push_back(nextEnd);
}
void removeThreePoints(std::vector<Point2D>& curve) {
    if (curve.size() <= 4) {
        return;
    }
    curve.pop_back();
    curve.pop_back();
    curve.pop_back();
}
int closestControlPoint(const std::vector<Point2D>& curve, Point2D mouse) {
    int best = 0;
    Point2D delta = curve[0] - mouse;
    float bestDistance = delta.x * delta.x + delta.y * delta.y;
    for (int i = 1; i < static_cast<int>(curve.size()); ++i) {
        delta = curve[static_cast<std::size_t>(i)] - mouse;
        const float distance = delta.x * delta.x + delta.y * delta.y;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

// TODO: (Part 1) Store four control points for the curve.
std::vector<Point2D> points = {
    {160.f, 620.f},
    {240.f, 180.f},
    {520.f, 180.f},
    {620.f, 620.f},
};

int selectedPoint = -1;

void moveSelectedPoint(Point2D mouse) {
    // handle edge cases
    if (selectedPoint < 0) {
        return;
    }
    if (selectedPoint >= static_cast<int>(points.size())) {
        selectedPoint = -1;
        return;
    }
    points[static_cast<std::size_t>(selectedPoint)] = mouse;
    keepHandleSmooth(points, static_cast<std::size_t>(selectedPoint));
}

void handleInput(sf::Window& window, bool& shouldQuit) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldQuit = true;
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            // TODO: (Part 3) On left-click, select the closest control point
            // using mouse->position and start dragging it.
            if (mouse->button == sf::Mouse::Button::Left) {
                const Point2D mousePosition = mouseToPoint(mouse->position);
                selectedPoint = closestControlPoint(points, mousePosition);
                moveSelectedPoint(mousePosition);
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
            // TODO: (Part 3) On left-button release, stop dragging.
            if (mouse->button == sf::Mouse::Button::Left) {
                selectedPoint = -1;
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            // TODO: (Part 3) Move the selected control point to mouse->position.
            // TODO: (Part 4) Maintain matching slopes at shared endpoints.
            // When moving point 3, move point 5 without changing its distance
            // from point 4 (point numbers here start at 1).
            moveSelectedPoint(mouseToPoint(mouse->position));
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            // TODO: (Part 4) '+' adds three control points; '-' removes three,
            // keeping at least four points.
            switch (key->code) {
                case sf::Keyboard::Key::Add:
                    addThreePoints(points);
                    break;
                case sf::Keyboard::Key::Equal:
                    if (key->shift) {
                        addThreePoints(points);
                    }
                    break;
                case sf::Keyboard::Key::Hyphen:
                case sf::Keyboard::Key::Subtract:
                    removeThreePoints(points);
                    if (selectedPoint >= static_cast<int>(points.size())) {
                        selectedPoint = -1;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void render(sf::RenderWindow& window) {
    window.clear(sf::Color::Black);

    static int elapsedFrames = 0;
    ++elapsedFrames;
    const float animationTime =
        static_cast<float>(elapsedFrames % TOTAL_FRAMES) / static_cast<float>(TOTAL_FRAMES);

    // ====== ====== ======
    // TODO: (Part 1) Sample GetPoint over t in [0, 1] and connect samples using the line-drawing
    // code from your project. Draw all four control points as circles after drawing the curve.
    // ====== ====== ======
    drawCurve(window, points, sf::Color::White);
    drawControlPoints(window, points, selectedPoint);

    // ====== ====== ======
    // TODO: (Part 2) Draw a small square moving repeatedly along the curve.
    // Use GetSlope to orient it to the curve at each time step.
    // ====== ====== ======
    drawOrientedSquare(window, points, animationTime);

    // ====== ====== ======
    // TODO: (Part 3) Draw control handles from point 1 to 2 and point 3 to 4.
    // TODO: (Part 4) Draw all connected cubic Bezier segments and their handles.
    // ====== ====== ======
    drawHandles(window, points);

    // ====== ====== ======
    // TODO: (Bonus) Support multiple curves, a Galaga screen overlay at a 1:2 ratio, and exporting
    // curve points as C++ code for Project 1b.
    // ====== ====== ======

    window.display();
}

int main() {
    sf::RenderWindow window;

    try {
        // Initialize window
        window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Bezier Curve Editor");
        window.setFramerateLimit(FPS_LIMIT);
        // Prevent key repeats.
        window.setKeyRepeatEnabled(false);

        bool shouldQuit = false;
        // Main game loop
        while (window.isOpen()) {
            handleInput(window, shouldQuit);
            if (shouldQuit) {
                break;
            }
            render(window);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
