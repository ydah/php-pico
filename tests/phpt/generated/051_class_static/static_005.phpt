--TEST--
static property method and constant vector 005
--FILE--
<?php
class GeneratedStatic005 {
    public const OFFSET = 6;
    public static $value = 16;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic005::add(3), ':', GeneratedStatic005::add(3), ':', GeneratedStatic005::$value;
--EXPECT--
25:28:22
