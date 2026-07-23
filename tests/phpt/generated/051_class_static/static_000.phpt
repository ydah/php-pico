--TEST--
static property method and constant vector 000
--FILE--
<?php
class GeneratedStatic000 {
    public const OFFSET = 1;
    public static $value = 1;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic000::add(2), ':', GeneratedStatic000::add(3), ':', GeneratedStatic000::$value;
--EXPECT--
4:7:6
