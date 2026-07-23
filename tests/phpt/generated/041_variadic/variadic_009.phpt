--TEST--
variadic argument collection vector 009
--FILE--
<?php
function generated_variadic_009($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_009(19, 10, 11, 12, 13, 14, 15, 16);
--EXPECT--
110
