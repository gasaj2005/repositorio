# Build Stage
FROM gcc:12 as builder

# Install CMake and Postgres deps
RUN apt-get update && apt-get install -y cmake libpq-dev libpqxx-dev

WORKDIR /app

# Copy source code
COPY . .

# Create build directory
RUN mkdir -p build
WORKDIR /app/build

# Configure and build
# We use -static-libstdc++ to make the executable more portable inside the container
RUN cmake .. && make

# Runner Stage
FROM debian:bookworm-slim

# Install runtime dependencies
# Crow might need some basic system libraries depending on linking, but usually libc is enough on ubuntu
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    ca-certificates \
    libpqxx-6.4 \
    libpq5 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy executable from builder
COPY --from=builder /app/build/Practica4IS .

# Copy static resources
COPY --from=builder /app/templates ./templates
COPY --from=builder /app/static ./static
COPY --from=builder /app/app.db .

# Expose the port (just for documentation, the app uses ENV)
EXPOSE 8080

# Run the app
CMD ["./Practica4IS"]
