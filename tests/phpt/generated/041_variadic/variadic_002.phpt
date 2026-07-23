--TEST--
variadic argument collection vector 002
--FILE--
<?php
function generated_variadic_002($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_002(5, 3, 4, 5, 6, 7);
--EXPECT--
30
