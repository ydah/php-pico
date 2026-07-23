--TEST--
static property method and constant vector 007
--FILE--
<?php
class GeneratedStatic007 {
    public const OFFSET = 8;
    public static $value = 22;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic007::add(5), ':', GeneratedStatic007::add(5), ':', GeneratedStatic007::$value;
--EXPECT--
35:40:32
