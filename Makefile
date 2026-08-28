# ---------------------------------------------------------------
# Terra – Terraria-style Platformer  |  Makefile
# Requires: g++ (C++17) and SDL2
#
# Install SDL2:
#   Ubuntu/Debian : sudo apt install libsdl2-dev
#   Fedora/RHEL   : sudo dnf install SDL2-devel
#   macOS (brew)  : brew install sdl2
#   Windows (MSYS2): pacman -S mingw-w64-x86_64-SDL2
#
# Build : make
# Run   : ./terra
# Debug : make debug && ./terra
# Clean : make clean
# ---------------------------------------------------------------

CXX      = g++
TARGET   = terra
SRCDIR   = src

# All game source files (main + every system)
SRCS = $(SRCDIR)/main.cpp        \
       $(SRCDIR)/Game.cpp        \
       $(SRCDIR)/World.cpp       \
       $(SRCDIR)/Noise.cpp       \
       $(SRCDIR)/TileType.cpp    \
       $(SRCDIR)/Item.cpp        \
       $(SRCDIR)/Inventory.cpp   \
       $(SRCDIR)/Player.cpp      \
       $(SRCDIR)/Enemy.cpp       \
       $(SRCDIR)/Particle.cpp    \
       $(SRCDIR)/BitmapFont.cpp  \
       $(SRCDIR)/SaveSystem.cpp

# sdl2-config provides the right -I and -L/-lSDL2 for any platform
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || echo "-I/usr/include/SDL2 -D_REENTRANT")
SDL_LIBS   := $(shell sdl2-config --libs   2>/dev/null || echo "-lSDL2")

COMMON_FLAGS = -std=c++17 -Wall -Wextra -I$(SRCDIR) $(SDL_CFLAGS)

# ---- Release (default) ----
CXXFLAGS = $(COMMON_FLAGS) -O2 -DNDEBUG

# ---- Debug ----
DEBUGFLAGS = $(COMMON_FLAGS) -O0 -g -fsanitize=address,undefined

.PHONY: all debug clean run tests

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $^ $(SDL_LIBS) -o $@
	@echo "Build successful → ./$(TARGET)"

debug: $(SRCS)
	$(CXX) $(DEBUGFLAGS) $^ $(SDL_LIBS) -o $(TARGET)_debug
	@echo "Debug build → ./$(TARGET)_debug"

run: all
	./$(TARGET)

# ---- Unit tests (no SDL dependency) ----
TESTSRCS_COMMON = $(SRCDIR)/World.cpp  $(SRCDIR)/Noise.cpp  $(SRCDIR)/TileType.cpp \
                  $(SRCDIR)/Item.cpp   $(SRCDIR)/Inventory.cpp

tests:
	@echo "--- World ---"
	$(CXX) $(COMMON_FLAGS) -O2 tests/test_world.cpp     $(TESTSRCS_COMMON) -o /tmp/t_world     && /tmp/t_world
	@echo "--- Player ---"
	$(CXX) $(COMMON_FLAGS) -O2 tests/test_player.cpp    $(TESTSRCS_COMMON) $(SRCDIR)/Player.cpp -o /tmp/t_player   && /tmp/t_player
	@echo "--- Inventory ---"
	$(CXX) $(COMMON_FLAGS) -O2 tests/test_inventory.cpp $(TESTSRCS_COMMON) -o /tmp/t_inventory && /tmp/t_inventory
	@echo "--- Enemy ---"
	$(CXX) $(COMMON_FLAGS) -O2 tests/test_enemy.cpp     $(TESTSRCS_COMMON) $(SRCDIR)/Player.cpp $(SRCDIR)/Enemy.cpp -o /tmp/t_enemy && /tmp/t_enemy
	@echo "--- SaveSystem ---"
	$(CXX) $(COMMON_FLAGS) -O2 tests/test_savesystem.cpp $(TESTSRCS_COMMON) $(SRCDIR)/Player.cpp $(SRCDIR)/SaveSystem.cpp -o /tmp/t_save && /tmp/t_save
	@echo "All test suites passed."

clean:
	rm -f $(TARGET) $(TARGET)_debug terra_save.dat /tmp/t_world /tmp/t_player /tmp/t_inventory /tmp/t_enemy /tmp/t_save
