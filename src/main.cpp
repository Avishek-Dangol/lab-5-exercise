#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int FPS_LIMIT = 30;
const int TOTAL_FRAMES = 90;

// Project 1a Galaga window. The editor view is twice this in each direction.
const float GALAGA_WIDTH = 768.f;
const float GALAGA_HEIGHT = 1024.f;

using Point2D = sf::Vector2f;

// mapPixelToCoords converts window pixels into the current view.
Point2D mouseToPoint(const sf::RenderWindow& window, sf::Vector2i position) {
    return window.mapPixelToCoords(position);
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

void drawGalagaOverlay(sf::RenderWindow& window) {
    sf::RectangleShape screen({GALAGA_WIDTH, GALAGA_HEIGHT});
    screen.setPosition({0.f, 0.f});
    screen.setFillColor(sf::Color(18, 22, 48));
    screen.setOutlineThickness(4.f);
    screen.setOutlineColor(sf::Color(180, 80, 255));
    window.draw(screen);
}

// TODO: (Part 1) Store four control points for the curve.
std::vector<std::vector<Point2D>> curves = {
    {
        {160.f, 620.f},
        {240.f, 180.f},
        {520.f, 180.f},
        {620.f, 620.f},
    },
};

int activeCurve = 0;
int selectedPoint = -1;

std::vector<Point2D>& currentCurve() { return curves[static_cast<std::size_t>(activeCurve)]; }
void selectCurve(int index) {
    const int count = static_cast<int>(curves.size());
    activeCurve = (index % count + count) % count;
    selectedPoint = -1;
}
void addCurve() {
    const float shift = 40.f * static_cast<float>(curves.size());
    curves.push_back({
        {180.f + shift, 700.f},
        {260.f + shift, 260.f},
        {500.f + shift, 260.f},
        {580.f + shift, 700.f},
    });
    activeCurve = static_cast<int>(curves.size()) - 1;
    selectedPoint = -1;
}
void removeActiveCurve() {
    if (curves.size() <= 1) {
        return;
    }
    curves.erase(curves.begin() + activeCurve);
    if (activeCurve >= static_cast<int>(curves.size())) {
        activeCurve = static_cast<int>(curves.size()) - 1;
    }
    selectedPoint = -1;
}

// Clicking picks the nearest control point on any curve and makes that curve active.
void selectClosestPoint(Point2D mouse) {
    int bestCurve = 0;
    int bestPoint = 0;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (int c = 0; c < static_cast<int>(curves.size()); ++c) {
        const auto& curve = curves[static_cast<std::size_t>(c)];
        const int point = closestControlPoint(curve, mouse);
        const Point2D delta = curve[static_cast<std::size_t>(point)] - mouse;
        const float distance = delta.x * delta.x + delta.y * delta.y;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestCurve = c;
            bestPoint = point;
        }
    }
    activeCurve = bestCurve;
    selectedPoint = bestPoint;
}

void moveSelectedPoint(Point2D mouse) {
    if (selectedPoint < 0) {
        return;
    }
    auto& curve = currentCurve();
    if (selectedPoint >= static_cast<int>(curve.size())) {
        selectedPoint = -1;
        return;
    }
    curve[static_cast<std::size_t>(selectedPoint)] = mouse;
    keepHandleSmooth(curve, static_cast<std::size_t>(selectedPoint));
}

std::string curvesAsCode() {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    out << "// Points outside 0.." << GALAGA_WIDTH << " or 0.." << GALAGA_HEIGHT
        << " are off-screen.\n";
    out << "std::vector<std::vector<sf::Vector2f>> curves = {\n";
    for (const auto& curve : curves) {
        out << "    {\n";
        for (const Point2D& point : curve) {
            out << "        {" << point.x << "f, " << point.y << "f},\n";
        }
        out << "    },\n";
    }
    out << "};\n";
    return out.str();
}

void exportCurves() {
    const std::string code = curvesAsCode();
    std::cout << code << std::flush;
    std::ofstream file("exported_curves.cpp");
    if (file) {
        file << code;
        std::cout << "//exported_curves.cpp in the working directory.\n";
    }
}

void handleInput(sf::RenderWindow& window, bool& shouldQuit) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldQuit = true;
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            // TODO: (Part 3) On left-click, select the closest control point
            // using mouse->position and start dragging it.
            if (mouse->button == sf::Mouse::Button::Left) {
                const Point2D mousePosition = mouseToPoint(window, mouse->position);
                selectClosestPoint(mousePosition);
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
            moveSelectedPoint(mouseToPoint(window, mouse->position));
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            // TODO: (Part 4) '+' adds three control points; '-' removes three,
            // keeping at least four points.
            switch (key->code) {
                case sf::Keyboard::Key::Add:
                    addThreePoints(currentCurve());
                    break;
                case sf::Keyboard::Key::Equal:
                    if (key->shift) {
                        addThreePoints(currentCurve());
                    }
                    break;
                case sf::Keyboard::Key::Hyphen:
                case sf::Keyboard::Key::Subtract:
                    removeThreePoints(currentCurve());
                    if (selectedPoint >= static_cast<int>(currentCurve().size())) {
                        selectedPoint = -1;
                    }
                    break;
                case sf::Keyboard::Key::N:
                    addCurve();
                    break;
                case sf::Keyboard::Key::LBracket:
                    selectCurve(activeCurve - 1);
                    break;
                case sf::Keyboard::Key::RBracket:
                    selectCurve(activeCurve + 1);
                    break;
                case sf::Keyboard::Key::Backspace:
                case sf::Keyboard::Key::Delete:
                    removeActiveCurve();
                    break;
                case sf::Keyboard::Key::E:
                    exportCurves();
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
    // TODO: (Bonus) Support multiple curves, a Galaga screen overlay at a 1:2 ratio, and exporting
    // curve points as C++ code for Project 1b.
    // ====== ====== ======
    drawGalagaOverlay(window);

    for (int c = 0; c < static_cast<int>(curves.size()); ++c) {
        const auto& curve = curves[static_cast<std::size_t>(c)];
        const int selected = (c == activeCurve) ? selectedPoint : -1;

        // ====== ====== ======
        // TODO: (Part 1) Sample GetPoint over t in [0, 1] and connect samples using the
        // line-drawing code from your project. Draw all four control points as circles after
        // drawing the curve.
        // ====== ====== ======
        drawCurve(window, curve, sf::Color::White);
        drawControlPoints(window, curve, selected);

        // ====== ====== ======
        // TODO: (Part 2) Draw a small square moving repeatedly along the curve.
        // Use GetSlope to orient it to the curve at each time step.
        // ====== ====== ======
        drawOrientedSquare(window, curve, animationTime);

        // ====== ====== ======
        // TODO: (Part 3) Draw control handles from point 1 to 2 and point 3 to 4.
        // TODO: (Part 4) Draw all connected cubic Bezier segments and their handles.
        // ====== ====== ======
        drawHandles(window, curve);
    }

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

        // View is twice the Galaga playfield (1:2), centered on it.
        // Viewport fits that 3:4 world into the 800x800 window without stretching.
        const float viewWidth = GALAGA_WIDTH * 2.f;
        const float viewHeight = GALAGA_HEIGHT * 2.f;
        sf::View view(sf::Vector2f(GALAGA_WIDTH * 0.5f, GALAGA_HEIGHT * 0.5f),
                      sf::Vector2f(viewWidth, viewHeight));
        const float windowRatio =
            static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
        const float viewRatio = viewWidth / viewHeight;
        const float viewportWidth = viewRatio / windowRatio;
        const float viewportLeft = (1.f - viewportWidth) * 0.5f;
        view.setViewport(sf::FloatRect({viewportLeft, 0.f}, {viewportWidth, 1.f}));
        window.setView(view);

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