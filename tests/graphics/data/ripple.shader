-- The ripple from the design: the content waves from side to side.
value Speed: number = 1
value Tint: color = #FFFFFF

function Pixel(input: PixelInput) returns color then
    constant wave = Sine(input.Position.X * 20 + input.Time * Speed)
    return Sample(input.Content, input.Coordinates + vector2(0, wave * 0.01)) * Tint
end
