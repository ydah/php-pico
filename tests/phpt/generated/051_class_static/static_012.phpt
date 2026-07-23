--TEST--
static property method and constant vector 012
--FILE--
<?php
class GeneratedStatic012 {
    public const OFFSET = 13;
    public static $value = 37;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic012::add(2), ':', GeneratedStatic012::add(5), ':', GeneratedStatic012::$value;
--EXPECT--
52:57:44
