--TEST--
variadic argument collection vector 001
--FILE--
<?php
function generated_variadic_001($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_001(3, 2, 3, 4, 5);
--EXPECT--
17
