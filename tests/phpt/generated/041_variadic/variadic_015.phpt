--TEST--
variadic argument collection vector 015
--FILE--
<?php
function generated_variadic_015($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_015(31, 16, 17, 18);
--EXPECT--
82
