--TEST--
static property method and constant vector 006
--FILE--
<?php
class GeneratedStatic006 {
    public const OFFSET = 7;
    public static $value = 19;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic006::add(4), ':', GeneratedStatic006::add(4), ':', GeneratedStatic006::$value;
--EXPECT--
30:34:27
