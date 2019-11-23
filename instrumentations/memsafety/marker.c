// Mark that the pointer may point to invalid memory
void __INSTR_mark_pointer(void)
{
	// this body will never by used
}

// Mark that the call to free may be invalid or may (not) take
// part in memory leaking
void __INSTR_mark_free(void)
{
	// this body will never by used
}

// Mark that this allocation may be leaked
void __INSTR_mark_allocation(void)
{
	// this body will never by used
}
// Mark that this exit may leak memory
void __INSTR_mark_exit(void)
{
	// this body will never by used
}
