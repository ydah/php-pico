--TEST--
variadic argument collection vector 010
--FILE--
<?php
function generated_variadic_010($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_010(21, 11, 12, 13);
--EXPECT--
57
