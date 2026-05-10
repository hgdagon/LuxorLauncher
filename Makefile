CXX = gcc
CXXFLAGS = -Wall -Wno-unknown-pragmas
LDLIBS = -Wl,-Bstatic -lstdc++ -lwinpthread -lole32 -luser32
TARGET = LuxorLauncher
STAGEDIR = staging
DISTRIBS = "Luxor" \
    "Luxor AR" \
    "Luxor2" \
    "Luxor3" \
    "LUXOR - Quest for the Afterlife" \
    "LUXOR - 5th Passage" \
    "LUXOR - Mahjong"

RELEASE =

ifeq ($(MAKECMDGOALS),release)
    RELEASE = -DRELEASE
endif

.PHONY: stage
all: $(TARGET)
release: $(TARGET) stage

$(TARGET): main.cpp
	@$(CXX) $(CXXFLAGS) $(RELEASE) -o $@ $^ $(LDLIBS)

stage: $(TARGET)
	@mkdir -p staging
	@for file in $(DISTRIBS); do \
		cp $^ "staging/$$file.exe"; \
		strip -s "staging/$$file.exe"; \
	done

clean:
	@rm -f $(TARGET)
	@rm -rf $(STAGEDIR)
