variable "VERSION" {
  default = "latest"
}

group "default" {
  targets = ["Toolchain"]
}

group "pir" {
  targets = ["Client", "Server"]
}

group "all" {
  targets = [
    "Toolchain",
    "Client", "Server",
    "Seperated", "Original"
  ]
}

function "getTag" {
  params = [target]
  result = ["spiral_${target}:${VERSION}"]
}

target "_spiral" {
  platforms = ["linux/amd64"]
}

target "Toolchain" {
  inherits = ["_spiral"]
  tags = getTag("toolchain")
}

target "Client" {
  inherits = ["_spiral"]
  dockerfile = "Client/Dockerfile"
  tags = getTag("client")
}

target "Server" {
  inherits = ["_spiral"]
  dockerfile = "PIR_Server/Dockerfile"
  tags = getTag("server")
}

target "Seperated" {
  inherits = ["_spiral"]
  dockerfile = "Seperated/Dockerfile"
  tags = getTag("seperated")
}

target "Original" {
  inherits = ["_spiral"]
  dockerfile = "Original/Dockerfile"
  tags = getTag("original")
}
