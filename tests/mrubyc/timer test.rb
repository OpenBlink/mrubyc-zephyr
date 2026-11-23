sleep_ms 20

values = [6, 7]
case values
in [left, right]
  $timer_result = left * right
end

def trace_inner
  raise RuntimeError, "exception trace check"
end

def trace_outer
  trace_inner
end

begin
  trace_outer
rescue RuntimeError => error
  $trace_result = error
end
