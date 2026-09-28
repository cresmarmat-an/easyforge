-- A shader with problems, which the tool must refuse.
function Pixel(input: PixelInput) returns color then
    return color(wave, 0, 0)
end
