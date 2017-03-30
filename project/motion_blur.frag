void main()
{
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = ftransform();
}
frag shader :
void main()
{
	vec2 uv = gl_TexCoord[0].st;
	vec4 color = vec4(0.0, 0.0, 0.0, 1.0);

	int j = 0;
	int sum = 0;
	for (int i = -kernel_size; i <= kernel_size; i++) {
		vec4 value = texture2D(texScreen, uv + mv);
		int factor = kernel_size + 1 - abs((float)i);
		color += value * factor;
		sum += factor;
	}
	color /= sum;

	gl_FragColor = color;
	gl_FragColor.a = 1.0;
}