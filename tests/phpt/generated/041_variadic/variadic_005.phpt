--TEST--
variadic argument collection vector 005
--FILE--
<?php
function generated_variadic_005($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_005(11, 6, 7, 8);
--EXPECT--
32
