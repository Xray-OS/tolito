# Toolchain settings
CXX      := g++
CXXFLAGS := -std=c++17 -I include -O2 -Wall -Wextra -Werror -MMD -MP

# Directories and targets
SRCDIR   := src
BUILDDIR := build
TARGET   := tolito

# Gather all .cpp files under src/
SOURCES  := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS  := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))
DEPS     := $(OBJECTS:.o=.d)

# Phony targets
.PHONY: all clean run

# Quiet mode toggle: set V=1 to see all commands
QUIET ?= @
ifeq ($(V),1)
QUIET :=
endif

# Default target
all: $(TARGET)

# Link step
$(TARGET): $(OBJECTS)
	$(QUIET)$(CXX) $(CXXFLAGS) $^ -o $@

# Compile step (generates .o and .d files)
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Include dependency files
-include $(DEPS)

# Run the tool
run: all
	@./$(TARGET)

# Clean build artifacts
clean:
	@rm -rf $(BUILDDIR) $(TARGET)
