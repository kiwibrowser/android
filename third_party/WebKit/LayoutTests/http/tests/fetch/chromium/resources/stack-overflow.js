// The bugs in this file are necessary to reproduce the crashes. Don't fix them.

// Run |func| with a full stack, so that any Javascript running inside it is hit
// with a stack overflow exception. |extraPadLevels| can be set to a non-zero
// value to add a bit more stack space. Sometimes this makes it possible to
// avoid crashing inside a small uninteresting function and instead crash inside
// the function of interest which is called later and requires more stack
// space. Useful |extraPadLevels| values are not portable and can only be
// determined by experiment.
function fillStackAndRun(func, extraPadLevels = 0) {
  try {
    padStack(extraPadLevels);
    fillStackAndRun(func, extraPadLevels);
  } catch (e) {
    return func();
  }
}

// Recurse |n| levels and then return. If it doesn't throw then an amount of
// stack space proportional to |n| is available.
function padStack(n) {
  if (n > 0) padStack(n - 1);
}
