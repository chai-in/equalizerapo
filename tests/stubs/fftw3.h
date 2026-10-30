#pragma once
typedef float fftwf_complex[2];
typedef void* fftwf_plan;
void fftwf_make_planner_thread_safe();
