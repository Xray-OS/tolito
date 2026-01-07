# Toolchain settings
CXX      := g++
CXXFLAGS := -std=c++17 -I include -O2 -Wall -Wextra -Werror -MMD -MP

# Redirect all compiler scratch files into a project-local tmp
TMPDIR := $(CURDIR)/build/tmp
export TMPDIR

# Directories and targets
SRCDIR   := src
BUILDDIR := build
TARGET   := tolito

# Gather all .cpp files under src/
SOURCES  := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS  := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))
DEPS     := $(OBJECTS:.o=.d)

# Phony targets
.PHONY: all clean run prepare-tmp

# Quiet mode toggle: set V=1 to see all commands
QUIET ?= @
ifeq ($(V),1)
QUIET :=
endif

# Default target
all: $(TARGET)

# Link step
$(TARGET): $(OBJECTS)
	$(QUIET)$(CXX) $(CXXFLAGS) $^ -o $@ -lcurl

# Create tmp + build directories before compiling
prepare-tmp:
	@mkdir -p $(TMPDIR) $(BUILDDIR)

# Compile step (generates .o and .d files)
# now depends on prepare-tmp so TMPDIR exists
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | prepare-tmp
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@

# Include dependency files
-include $(DEPS)

# Run the tool
run: all
	@./$(TARGET)

# Clean build artifacts
clean:
	@rm -rf $(BUILDDIR) $(TARGET)
