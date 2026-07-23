--TEST--
finally mutation of global state is visible
--FILE--
<?php
$final_global = 2;
function final_mutate_global() {
    global $final_global;
    try { $final_global += 3; } finally { $final_global *= 4; }
}
final_mutate_global(); echo $final_global;
--EXPECT--
20
