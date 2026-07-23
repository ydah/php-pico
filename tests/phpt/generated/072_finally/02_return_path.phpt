--TEST--
finally executes on a return path
--FILE--
<?php
function final_return_path() {
    try { return 7; } finally { echo 'finally:'; }
}
echo final_return_path();
--EXPECT--
finally:7
